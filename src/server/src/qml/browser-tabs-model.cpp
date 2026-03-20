#include "browser-tabs-model.hpp"
#include "actions/browser-tab-actions.hpp"
#include <algorithm>

void BrowserTabsModel::applyFilter() {
  m_filtered.clear();
  m_filtered.reserve(m_items.size());

  if (m_query.empty()) {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
      m_filtered.push_back({.data = i, .score = 0});
    }
  } else {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
      auto const &tab = m_items[i];
      int score = tab.fuzzyScore(m_query, m_browserService.prioritizeUrlMatches());
      if (score > 0) { m_filtered.push_back({.data = i, .score = score}); }
    }

    std::stable_sort(m_filtered.begin(), m_filtered.end(), std::greater{});
  }

  QString label = sectionLabel().replace("{count}", QString::number(m_filtered.size()));
  commitSections(m_filtered.empty()
                     ? std::vector<SectionInfo>{}
                     : std::vector<SectionInfo>{{.name = label, .count = int(m_filtered.size())}});
}

QString BrowserTabsModel::displayTitle(const BrowserTab &tab) const {
  return QString::fromStdString(tab.title);
}

QString BrowserTabsModel::displaySubtitle(const BrowserTab &tab) const {
  return QString::fromStdString(tab.url);
}

QString BrowserTabsModel::displayIconSource(const BrowserTab &tab) const {
  return imageSourceFor(tab.icon());
}

QVariantList BrowserTabsModel::displayAccessory(const BrowserTab &tab) const {
  if (tab.audible) return qml::textAccessory(tab.muted ? QStringLiteral("Muted") : QStringLiteral("Playing"));
  return {};
}

std::unique_ptr<ActionPanelState> BrowserTabsModel::buildActionPanel(const BrowserTab &tab) const {
  return BrowserTabActionGenerator::generate(scope().appContext(), tab);
}

QString BrowserTabsModel::sectionLabel() const { return QStringLiteral("Tabs ({count})"); }
