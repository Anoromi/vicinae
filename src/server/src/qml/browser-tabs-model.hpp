#pragma once
#include "fuzzy-list-model.hpp"
#include "services/browser-extension-service.hpp"

using BrowserTab = BrowserExtensionService::BrowserTab;

template <> struct fuzzy::FuzzySearchable<BrowserTab> {
  static int score(const BrowserTab &tab, std::string_view query) { return tab.fuzzyScore(query, false); }
};

class BrowserTabsModel : public FuzzyListModel<BrowserTab> {
  Q_OBJECT

public:
  BrowserTabsModel(BrowserExtensionService &browserService, QObject *parent = nullptr)
      : FuzzyListModel(parent), m_browserService(browserService) {}

protected:
  void applyFilter() override;
  QString displayTitle(const BrowserTab &tab) const override;
  QString displaySubtitle(const BrowserTab &tab) const override;
  QString displayIconSource(const BrowserTab &tab) const override;
  QVariantList displayAccessory(const BrowserTab &tab) const override;
  std::unique_ptr<ActionPanelState> buildActionPanel(const BrowserTab &tab) const override;
  QString sectionLabel() const override;

private:
  BrowserExtensionService &m_browserService;
};
