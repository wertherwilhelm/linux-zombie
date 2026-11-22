#include <gtest/gtest.h>
#include "REPL-test/scheme_input.hpp"

TEST(ReplInput, ContinuesIncompleteForms) {
    for (auto input : {"(define (f x)", "\"hello", "'", "(list '"})
        EXPECT_TRUE(scheme_input::scan(input).incomplete) << input;
}
TEST(ReplInput, IgnoresDelimitersInStringsAndComments) {
    auto a = scheme_input::scan("(list \"hello\n)world\" ; (ignored\n 1)");
    EXPECT_FALSE(a.incomplete);
    EXPECT_EQ(a.depth, 0);
    EXPECT_FALSE(a.error);
}
TEST(ReplInput, CharacterLiteralsAreNotSyntax) {
    for (auto input : {"#\\(", "#\\)", "#\\;", "#\\\"", "(list #\\( #\\))", "#\\space"}) {
        auto a = scheme_input::scan(input);
        EXPECT_FALSE(a.incomplete) << input;
        EXPECT_FALSE(a.error) << input;
        EXPECT_EQ(a.depth, 0) << input;
    }
}
TEST(ReplInput, EscapesAndQuotedIdentifiers) {
    EXPECT_FALSE(scheme_input::scan("\"a\\\"(b\"").incomplete);
    EXPECT_TRUE(scheme_input::scan("\"a\\\"").incomplete);
    EXPECT_FALSE(scheme_input::scan("|hello(world|").incomplete);
}
TEST(ReplInput, NestedBlockComments) {
    EXPECT_TRUE(scheme_input::scan("#| outer #| inner |#").incomplete);
    auto a = scheme_input::scan("#| outer #| (inner |# ) |#");
    EXPECT_FALSE(a.incomplete);
    EXPECT_FALSE(a.has_datum);
    EXPECT_EQ(a.depth, 0);
}
TEST(ReplInput, MalformedDelimitersReachReaderImmediately) {
    for (auto input : {")", "([)", "(')"}) {
        auto a = scheme_input::scan(input);
        EXPECT_TRUE(a.error) << input;
        EXPECT_FALSE(a.incomplete) << input;
    }
}
TEST(ReplInput, VectorsAndMultilineQuotes) {
    EXPECT_TRUE(scheme_input::scan("#(1 2").incomplete);
    EXPECT_FALSE(scheme_input::scan("#u8(1 2)").incomplete);
    EXPECT_TRUE(scheme_input::scan("' ; comment").incomplete);
    EXPECT_FALSE(scheme_input::scan("' ; comment\nhello").incomplete);
}

#include "REPL-test/repl_core.hpp"

namespace {
class RecordingEngine : public IEvalEngine {
public:
    std::vector<std::string> sources;
    void reset() override {}
    void* eval_string(std::string_view src) override {
        sources.emplace_back(src);
        return this;
    }
    std::string print_value(void*) override { return "ok"; }
};
}
TEST(ReplSessionInput, MultilineStringPreservesCommandLookingLines) {
    auto engine = std::make_unique<RecordingEngine>();
    auto* recording = engine.get();
    ReplSession session(std::move(engine));
    session.submit_line("\"hello");
    session.submit_line(":clear");
    session.submit_line("exit");
    EXPECT_TRUE(recording->sources.empty());
    session.submit_line("world\"");
    ASSERT_EQ(recording->sources.size(), 1u);
    EXPECT_EQ(recording->sources[0], "\"hello\n:clear\nexit\nworld\"\n");
}
TEST(ReplSessionInput, CharacterLiteralDoesNotStrandSession) {
    auto engine = std::make_unique<RecordingEngine>();
    auto* recording = engine.get();
    ReplSession session(std::move(engine));
    session.submit_line("#\\(");
    ASSERT_EQ(recording->sources.size(), 1u);
    session.submit_line("(+ 1");
    EXPECT_EQ(recording->sources.size(), 1u);
    session.submit_line("2)");
    ASSERT_EQ(recording->sources.size(), 2u);
    EXPECT_EQ(recording->sources[1], "(+ 1\n2)\n");
}
TEST(ReplSessionInput, ClearDiscardsUnfinishedInput) {
    auto engine = std::make_unique<RecordingEngine>();
    auto* recording = engine.get();
    ReplSession session(std::move(engine));
    session.submit_line("(unfinished");
    session.clear_buffer();
    session.submit_line("42");
    ASSERT_EQ(recording->sources.size(), 1u);
    EXPECT_EQ(recording->sources[0], "42\n");
}
