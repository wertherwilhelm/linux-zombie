/**
 * @file CorpusTests.cpp
 * @brief Data-driven corpus runner.
 *
 * Each subdirectory of corpus/ is a test entry with:
 *   source.scm  - library code loaded first (e.g. a reader / language front-end)
 *   cases.scm   - defines `corpus-cases`: a list of (label form expected) triples
 *                 (`form` is a quoted expression; `expected` a datum). See
 *                 corpus/algol-msweet/cases.scm for the format.
 *
 * The runner loads source + cases into a fresh scheme_engine, then evaluates each
 * `form` and compares its printed result to `expected`. A case that mismatches OR
 * raises an error is reported (errors are how the corpus discovers missing /
 * under-implemented areas by exercising real programs).
 *
 * Add a test = add a corpus entry; no recompile needed to add cases.
 */
#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "scheme_engine.h"
#include "native_compiler.h"
#include "constants.h"
#include <filesystem>
#include <string>
#include <vector>

#ifndef ZOMBIE_CORPUS_DIR
#define ZOMBIE_CORPUS_DIR "corpus"
#endif

namespace fs = std::filesystem;

namespace {

// Evaluate a pre-parsed datum, returning its printed form or an "<error: ...>"
// marker. Never throws — a failing case must not abort the whole corpus.
std::string eval_show(scheme_engine& engine, Object* form) {
    try {
        Object* r = engine.eval_expr(form);
        if (!r) {
            std::string e = engine.get_last_error();
            return e.empty() ? std::string("<null>") : ("<error: " + e + ">");
        }
        return engine.print_value(r);
    } catch (const std::exception& e) {
        return std::string("<error: ") + e.what() + ">";
    } catch (...) {
        return "<error: unknown>";
    }
}

std::string as_text(scheme_engine& engine, Object* o) {
    if (auto* s = dynamic_cast<a_string*>(o)) return s->value();
    return engine.print_value(o);
}

const char* eval_label(EvaluatorType type) {
    switch (type) {
    case EvaluatorType::LISP:     return "lisp";
    case EvaluatorType::TOY_VM:   return "vm";
    case EvaluatorType::BYTECODE: return "bytecode";
    case EvaluatorType::COREIR:   return "coreir";
    case EvaluatorType::NATIVE:   return "native";
    }
    return "?";
}

// Run one corpus directory against one evaluator. Reports each case via EXPECT.
// `reference` is the tree-walk run that authored the 'gap tags: only it flags a
// gap that has started passing. For other evaluators (e.g. native) a passing gap
// is merely noted, since the tag is reference-relative.
void run_corpus_dir(const fs::path& dir, EvaluatorType type, bool reference) {
    const std::string name = dir.filename().string();
    const std::string ev = eval_label(type);
    const std::string source = (dir / "source.scm").string();
    const std::string cases = (dir / "cases.scm").string();

    scheme_engine engine(type);

    // source.scm is optional: a feature corpus may rely only on builtins.
    if (fs::exists(source) && !engine.eval_string("(load \"" + source + "\")")) {
        ADD_FAILURE() << "[" << ev << " " << name << "] failed to load source.scm: "
                      << engine.get_last_error();
        return;
    }
    if (!engine.eval_string("(load \"" + cases + "\")")) {
        ADD_FAILURE() << "[" << ev << " " << name << "] failed to load cases.scm: "
                      << engine.get_last_error();
        return;
    }

    Object* list = static_cast<Object*>(engine.eval_string("corpus-cases"));
    if (!list || list->is_nil()) {
        ADD_FAILURE() << "[" << name << "] corpus-cases is empty or undefined";
        return;
    }

    int n = 0;
    for (Object* c = list; c && !c->is_nil(); c = cdr(c)) {
        Object* entry = car(c);
        if (!entry || entry->is_atom()) {
            ADD_FAILURE() << "[" << name << "] malformed case entry";
            continue;
        }
        Object* label = car(entry);
        Object* form = car(cdr(entry));
        Object* expected = car(cdr(cdr(entry)));

        // Optional 4th element: a status tag. 'gap marks a known
        // missing/under-implemented area — it is reported but does NOT fail the
        // suite, and if it starts passing the runner flags it so the tag can be
        // dropped. (This makes the corpus both a regression suite and a living
        // gap tracker — the point of running real programs to find gaps.)
        bool is_gap = false;
        Object* rest4 = cdr(cdr(cdr(entry)));
        if (rest4 && !rest4->is_nil() && rest4->pair()) {
            is_gap = (as_text(engine, car(rest4)) == "gap");
        }

        std::string lbl = as_text(engine, label);
        std::string want = engine.print_value(expected);
        std::string got = eval_show(engine, form);

        if (is_gap) {
            if (got == want) {
                if (reference) {
                    ADD_FAILURE() << "corpus " << name << " / case " << lbl
                                  << ": tagged 'gap but now PASSES — drop the tag";
                } else {
                    std::cout << "[gap-pass] " << ev << " " << name << "/" << lbl
                              << ": tagged 'gap but passes here\n";
                }
            } else {
                std::cout << "[gap] " << ev << " " << name << "/" << lbl
                          << ": got " << got << " want " << want << "\n";
            }
        } else {
            EXPECT_EQ(got, want) << "corpus " << name << " / case " << lbl
                                 << " [" << ev << "]";
        }
        ++n;
    }
    std::cout << "[corpus] " << ev << " " << name << ": ran " << n << " cases\n";
}

std::vector<fs::path> discover_corpus_dirs() {
    std::vector<fs::path> dirs;
    std::error_code ec;
    fs::path root(ZOMBIE_CORPUS_DIR);
    if (!fs::is_directory(root, ec)) return dirs;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (e.is_directory() && fs::exists(e.path() / "cases.scm")) {
            dirs.push_back(e.path());
        }
    }
    std::sort(dirs.begin(), dirs.end());
    return dirs;
}

} // namespace

class CorpusTest : public ::testing::Test {
protected:
    void SetUp() override { init_lisp_constants(); }
};

TEST_F(CorpusTest, AllCorpusEntries) {
    auto dirs = discover_corpus_dirs();
    ASSERT_FALSE(dirs.empty())
        << "no corpus entries found under " << ZOMBIE_CORPUS_DIR;
    for (const auto& dir : dirs) {
        run_corpus_dir(dir, EvaluatorType::LISP, /*reference=*/true);
    }
}

// The native JIT (CoreIR -> asmjit, bytecode fallback) must reproduce every
// non-gap corpus result the tree-walk reference produces. Skipped when asmjit is
// disabled or the host is not supported.
TEST_F(CorpusTest, NativeMatchesExpected) {
    if (!zombie::native::NativeCompilerBackend::available()) {
        GTEST_SKIP() << "native backend unavailable (asmjit disabled or non-x86-64)";
    }
    auto dirs = discover_corpus_dirs();
    ASSERT_FALSE(dirs.empty())
        << "no corpus entries found under " << ZOMBIE_CORPUS_DIR;
    for (const auto& dir : dirs) {
        run_corpus_dir(dir, EvaluatorType::NATIVE, /*reference=*/false);
    }
}
