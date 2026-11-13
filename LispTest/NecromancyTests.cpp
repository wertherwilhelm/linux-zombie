/**
 * @file NecromancyTests.cpp
 * @brief Tests for the GC graveyard ("Last Words") system
 *
 * graveyard / epitaph / death-toll / seance / gc-collect
 * Gothic costume, GC observability underneath (see last_words.h).
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include "last_words.h"

class NecromancyTest : public ZombieSchemeTest {
protected:
    // Flush garbage accumulated by earlier tests (dead fixture envs etc.)
    // so ring-buffer assertions see only this test's deaths.
    void flushGraveyard() {
        eval("(gc-collect)");
    }

    // Walk a Scheme list of strings looking for an exact entry
    bool graveyardContains(const std::string& text) {
        Object* graves = eval("(graveyard)");
        for (Object* g = graves; g && !is_nil(g); g = cdr(g)) {
            a_string* line = dynamic_cast<a_string*>(car(g));
            if (line && line->value() == text) {
                return true;
            }
        }
        return false;
    }
};

TEST_F(NecromancyTest, PrimitivesExist) {
    EXPECT_TRUE(primitiveExists("graveyard"));
    EXPECT_TRUE(primitiveExists("epitaph"));
    EXPECT_TRUE(primitiveExists("death-toll"));
    EXPECT_TRUE(primitiveExists("seance"));
    EXPECT_TRUE(primitiveExists("gc-collect"));
}

TEST_F(NecromancyTest, GcCollectReturnsReclaimCount) {
    Object* result = eval("(gc-collect)");
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_GE(numeric::to_int64(result), 0);
}

TEST_F(NecromancyTest, DeathTollIncreasesWhenGarbageIsCollected) {
    flushGraveyard();
    Object* before = eval("(death-toll)");
    int64_t t0 = numeric::to_int64(before);

    // Create garbage: the cons is unreachable as soon as begin discards it
    eval("(begin (cons 'doomed 'pair) #f)");
    eval("(gc-collect)");

    Object* after = eval("(death-toll)");
    EXPECT_GT(numeric::to_int64(after), t0);
}

TEST_F(NecromancyTest, GraveyardReturnsListOfStrings) {
    flushGraveyard();
    eval("(begin (cons 1 2) #f)");
    eval("(gc-collect)");

    Object* graves = eval("(graveyard)");
    ASSERT_NE(graves, nullptr);
    ASSERT_TRUE(graves->pair()) << "graveyard should be non-empty after a collection";

    for (Object* g = graves; g && !is_nil(g); g = cdr(g)) {
        EXPECT_NE(dynamic_cast<a_string*>(car(g)), nullptr)
            << "every grave should be a string";
    }
}

TEST_F(NecromancyTest, EpitaphIsSpokenAtDeath) {
    flushGraveyard();
    eval("(epitaph (cons 13 13) \"here lies a humble pair, built and abandoned\")");
    eval("(gc-collect)");

    EXPECT_TRUE(graveyardContains("here lies a humble pair, built and abandoned"));
}

TEST_F(NecromancyTest, EpitaphReturnsTheObject) {
    // epitaph passes its subject through, so it composes
    expectInt("(car (epitaph (cons 7 8) \"doomed\"))", 7);
}

TEST_F(NecromancyTest, DefinedBindingsSurviveCollection) {
    // C++-level: a value bound via env->define must survive a collection
    // with the same identity, and its epitaph must stay unspoken
    namespace necro = zombie::runtime::necromancy;
    flushGraveyard();

    Object* c = cons(gc_new<a_symbol>("still"), gc_new<a_symbol>("alive"));
    necro::set_epitaph(c, "binding died: define does not protect its value");
    env->define(gc_new<a_string>("bound-survivor"), c);

    gc->collect();

    EXPECT_EQ(eval("bound-survivor"), c) << "binding lost or replaced by collection";
    EXPECT_FALSE(graveyardContains("binding died: define does not protect its value"));
}

TEST_F(NecromancyTest, EpitaphSurvivorsSayNothing) {
    flushGraveyard();
    // A rooted (defined) object must NOT appear in the graveyard.
    // Subtlety this test once tripped over: the SOURCE LITERAL "spoke too
    // soon" inside the parsed AST also dies after evaluation, and its
    // natural epitaph would read identically if strings were unquoted.
    // describe() quotes strings, so the literal's grave is "\"spoke too
    // soon\"" and exact match below can only see the custom words.
    eval("(define survivor (epitaph (cons 'still 'alive) \"spoke too soon\"))");
    eval("(gc-collect)");

    EXPECT_FALSE(graveyardContains("spoke too soon"));
    // ...and it is still usable
    expectSymbol("(car survivor)", "still");
}

TEST_F(NecromancyTest, EpitaphRequiresString) {
    EXPECT_THROW(eval("(epitaph (cons 1 2) 42)"), std::runtime_error);
}

TEST_F(NecromancyTest, AnonymousDeadGetTypeLabels) {
    flushGraveyard();
    eval("(begin (cons 'a 'b) #f)");
    eval("(gc-collect)");

    EXPECT_TRUE(graveyardContains("#<pair>"));
}

TEST_F(NecromancyTest, SeanceReturnsAReport) {
    flushGraveyard();
    eval("(begin (cons 1 2) #f)");
    eval("(gc-collect)");

    Object* result = eval("(seance)");
    a_string* report = dynamic_cast<a_string*>(result);
    ASSERT_NE(report, nullptr);
    EXPECT_FALSE(report->value().empty());
    EXPECT_NE(report->value().find("souls"), std::string::npos);
}
