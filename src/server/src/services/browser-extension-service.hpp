#pragma once
#include <QObject>
#include <algorithm>
#include <cstdint>
#include <glaze/core/reflect.hpp>
#include <ranges>
#include "lib/fuzzy/fuzzy-searchable.hpp"
#include "ui/image/url.hpp"
#include "vicinae-ipc/ipc.hpp"

class BrowserExtensionService : public QObject {
  Q_OBJECT

public:
  enum class TabAction : std::uint8_t { Focus, Close };

signals:
  void tabActionRequested(std::string browserId, int id, TabAction action) const;
  void tabsChanged() const;
  void browsersChanged() const;
  void rankingModeChanged() const;

public:
  struct BrowserTab : ipc::BrowserTabInfo {
    std::string browserId;

    std::string uniqueId() const { return std::format("{}-{}", browserId, id); }
    std::string hostKey() const {
      QUrl const qurl(url.c_str());
      if (!qurl.isValid() || !std::ranges::contains(std::initializer_list{"https", "http"}, qurl.scheme())) {
        return {};
      }

      auto host = qurl.host().toLower();
      if (host.startsWith("www.")) { host.remove(0, 4); }
      return host.toStdString();
    }
    int fuzzyScore(std::string_view query, bool prioritizeUrlMatches) const {
      return prioritizeUrlMatches ? fuzzy::scoreWeighted({{hostKey(), 1.0}, {url, 0.7}, {title, 0.3}}, query)
                                  : fuzzy::scoreWeighted({{title, 1.0}, {url, 0.7}}, query);
    }
    QString searchableTitle(bool prioritizeUrlMatches) const {
      if (auto host = hostKey(); prioritizeUrlMatches && !host.empty()) {
        return QString::fromStdString(host);
      }
      return QString::fromStdString(title);
    }
    QString searchableSubtitle(bool prioritizeUrlMatches) const {
      return prioritizeUrlMatches ? QString::fromStdString(url) : QString();
    }
    std::vector<QString> keywords(bool prioritizeUrlMatches) const {
      if (!prioritizeUrlMatches) { return {QString::fromStdString(url)}; }
      return {QString::fromStdString(title), QString::fromStdString(url)};
    }
    double searchScoreWeight(bool prioritizeUrlMatches) const { return prioritizeUrlMatches ? 1.15 : 1.0; }

    ImageURL icon() const {
      if (QUrl qurl = QUrl(url.c_str());
          qurl.isValid() && std::ranges::contains(std::initializer_list{"https", "http"}, qurl.scheme())) {
        return ImageURL::favicon(qurl.host()).withFallback(BuiltinIcon::AppWindowSidebarLeft);
      }
      return BuiltinIcon::AppWindowSidebarLeft;
    }
  };

  struct BrowserInfo {
    std::string id;
    std::string name;
    std::string engine;
    std::vector<BrowserTab> tabs;
  };

  BrowserExtensionService() = default;

  bool prioritizeUrlMatches() const { return m_prioritizeUrlMatches; }

  void setPrioritizeUrlMatches(bool value) {
    if (m_prioritizeUrlMatches == value) return;
    m_prioritizeUrlMatches = value;
    emit rankingModeChanged();
  }

  std::vector<BrowserTab> tabs() const {
    return m_browsers | std::views::transform([](auto &&b) { return b.tabs; }) | std::views::join |
           std::ranges::to<std::vector>();
  }

  auto findById(const std::string &id) {
    return std::ranges::find_if(m_browsers, [&](auto &&browser) { return browser.id == id; });
  }

  void setTabs(std::string id, std::vector<ipc::BrowserTabInfo> tabs) {
    if (auto it = findById(id); it != m_browsers.end()) {
      it->tabs = tabs | std::views::transform([&](const ipc::BrowserTabInfo &info) {
                   auto tab = BrowserTab(info);
                   tab.browserId = id;
                   return tab;
                 }) |
                 std::ranges::to<std::vector>();
      emit tabsChanged();
    }
  }

  void focusTab(const std::string &browserId, int tabId) {
    emit tabActionRequested(browserId, tabId, TabAction::Focus);
  }

  void focusTab(const BrowserTab &tab) { focusTab(tab.browserId, tab.id); }

  std::expected<void, std::string> closeTab(const std::string &browserId, int tabId) {
    // remove it immediately, don't wait for the next tab change event so that root search
    // is updated immediately.

    auto browserIt = std::ranges::find_if(m_browsers, [&](auto &&info) { return info.id == browserId; });

    if (browserIt == m_browsers.end()) { return std::unexpected("No such browser"); }

    auto tabIt = std::ranges::find_if(browserIt->tabs, [&](auto &&tab) { return tab.id == tabId; });

    if (tabIt == browserIt->tabs.end()) { return std::unexpected("No such tab"); }

    browserIt->tabs.erase(tabIt);

    emit tabsChanged();
    emit tabActionRequested(browserId, tabId, TabAction::Close);

    return {};
  }

  auto closeTab(const BrowserTab &tab) { return closeTab(tab.browserId, tab.id); }

  /**
   * Find the first active tab. If many browsers are connected, the active tab from the first connected
   * browser is returned.
   */
  const BrowserTab *findActiveTab() {
    for (const auto &browser : m_browsers) {
      if (auto it = std::ranges::find_if(browser.tabs, [](auto &&tab) { return tab.active; });
          it != browser.tabs.end()) {
        return &*it;
      }
    }
    return nullptr;
  }

  void registerBrowser(const BrowserInfo &info) {
    if (auto it = findById(info.id); it != m_browsers.end()) {
      *it = info;
    } else {
      m_browsers.emplace_back(info);
    }
    emit browsersChanged();
  }

  void unregisterBrowser(std::string id) {
    if (auto it = findById(id); it != m_browsers.end()) {
      m_browsers.erase(it);
      emit tabsChanged();
      emit browsersChanged();
    }
  }

  std::span<const BrowserInfo> browsers() const { return m_browsers; }

private:
  std::vector<BrowserInfo> m_browsers;
  bool m_prioritizeUrlMatches = false;
};
