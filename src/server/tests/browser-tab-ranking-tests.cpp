#include <catch2/catch_test_macros.hpp>
#include "lib/fts_fuzzy.hpp"
#include "services/browser-extension-service.hpp"

namespace {

using BrowserTab = BrowserExtensionService::BrowserTab;

BrowserTab makeTab(std::string title, std::string url, int id = 1) {
  BrowserTab tab;
  tab.id = id;
  tab.title = std::move(title);
  tab.url = std::move(url);
  return tab;
}

double rootSearchScore(const BrowserTab &tab, std::string_view query, bool prioritizeUrlMatches) {
  int best = 0;
  auto scoreField = [&](std::string_view text, double weight) {
    int score = 0;
    if (fts::fuzzy_match(query, text, score)) { best = std::max(best, int(score * weight)); }
  };

  scoreField(tab.searchableTitle(prioritizeUrlMatches).toStdString(), 1.0);
  scoreField(tab.searchableSubtitle(prioritizeUrlMatches).toStdString(), 0.6);
  for (const auto &keyword : tab.keywords(prioritizeUrlMatches)) {
    scoreField(keyword.toStdString(), 0.3);
  }

  return best * tab.searchScoreWeight(prioritizeUrlMatches);
}

double rootSearchScore(std::string_view title, std::string_view query) {
  int score = 0;
  return fts::fuzzy_match(query, title, score) ? score : 0;
}

} // namespace

TEST_CASE("browser tab hostKey normalizes supported URLs") {
  REQUIRE(makeTab("Linear", "https://www.linear.app/foo").hostKey() == "linear.app");
  REQUIRE(makeTab("GitHub", "https://github.com/openai").hostKey() == "github.com");
  REQUIRE(makeTab("Invalid", "notaurl").hostKey().empty());
  REQUIRE(makeTab("Blank", "about:blank").hostKey().empty());
}

TEST_CASE("browser tab root search fields follow ranking mode") {
  auto tab = makeTab("Inbox", "https://linear.app/acme/issue/1");

  REQUIRE(tab.searchableTitle(false) == "Inbox");
  REQUIRE(tab.searchableSubtitle(false).isEmpty());
  REQUIRE(tab.keywords(false) == std::vector<QString>{QStringLiteral("https://linear.app/acme/issue/1")});
  REQUIRE(tab.searchScoreWeight(false) == 1.0);

  REQUIRE(tab.searchableTitle(true) == "linear.app");
  REQUIRE(tab.searchableSubtitle(true) == "https://linear.app/acme/issue/1");
  REQUIRE(tab.keywords(true) ==
          std::vector<QString>{QStringLiteral("Inbox"), QStringLiteral("https://linear.app/acme/issue/1")});
  REQUIRE(tab.searchScoreWeight(true) == 1.15);
}

TEST_CASE("browser tab ranking mode favors hostname matches in root search") {
  auto linearTab = makeTab("Inbox", "https://linear.app/acme/issue/1", 1);
  auto docsTab = makeTab("Linear docs mirror", "https://docs.example.com/article", 2);

  REQUIRE(rootSearchScore(docsTab, "linear", false) > rootSearchScore(linearTab, "linear", false));
  REQUIRE(rootSearchScore("Search Linear", "linear") > rootSearchScore(linearTab, "linear", false));

  REQUIRE(rootSearchScore(linearTab, "linear", true) > rootSearchScore(docsTab, "linear", true));
  REQUIRE(rootSearchScore(linearTab, "linear", true) > rootSearchScore("Search Linear", "linear"));
}

TEST_CASE("browser tabs view scoring reprioritizes host matches") {
  auto article = makeTab("Linear pricing article", "https://example.com/blog/linear", 1);
  auto linear = makeTab("Inbox", "https://linear.app/acme/issue/1", 2);
  auto github = makeTab("Repo", "https://github.com/openai/openai", 3);

  REQUIRE(article.fuzzyScore("linear", false) > linear.fuzzyScore("linear", false));
  REQUIRE(linear.fuzzyScore("linear", true) > article.fuzzyScore("linear", true));
  REQUIRE(github.fuzzyScore("github", true) > article.fuzzyScore("github", true));
}

TEST_CASE("browser extension service only emits ranking changes on actual toggles") {
  BrowserExtensionService service;
  int signalCount = 0;
  QObject::connect(&service, &BrowserExtensionService::rankingModeChanged,
                   [&signalCount]() { ++signalCount; });

  service.setPrioritizeUrlMatches(false);
  service.setPrioritizeUrlMatches(true);
  service.setPrioritizeUrlMatches(true);
  service.setPrioritizeUrlMatches(false);

  REQUIRE(signalCount == 2);
}
