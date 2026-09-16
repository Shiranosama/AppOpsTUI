#ifndef APPOPS_TUI
#define APPOPS_TUI

#include "appops_data.h"
#include "appops_path.h"
#include "termbox2.h"

// PermissionManager类
class PermissionManager {
 public:
  PermissionManager() = default;

  // 获取并解析软件包列表
  void fetchPackages() {
    appList_.clear();
    for (const auto& pkg : getApplist()) {
      appList_.emplace(pkg, CmdAppops(pkg));
    }
  }

  // 从文件加载描述，返回加载的条目数
  int loadDescriptions(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return 0;

    std::string line;
    int count = 0;
    while (std::getline(f, line)) {
      if (line.empty() || line[0] == '#') continue;

      auto sep = line.find('|');
      if (sep == std::string::npos) continue;

      std::string op = line.substr(0, sep);
      std::string desc = line.substr(sep + 1);
      opDescriptions_[op] = desc;
      count++;
    }
    return count;
  }

  // 打印软件列表与TUI，开始监听事件
  void showPackages() {
    if (appList_.empty()) {
      fetchPackages();
    }
    if (appList_.empty()) {
      std::fprintf(stderr,
                   "[Error] No packages found. Make sure `pm list packages`"
                   " works (adb/root shell required).\n");
      return;
    }

    if (tb_init() != TB_OK) {
      std::fprintf(stderr, "[Error] tb_init: %s\n",
                   tb_strerror(tb_last_errno()));
      return;
    }
    tb_set_input_mode(TB_INPUT_ESC);
    tb_hide_cursor();

    bool quit = false;
    bool confirmQuit = false;
    bool searchMode = false;
    size_t selected = 0;
    int scroll = 0;
    std::string filter;
    std::string message;
    std::vector<std::string> visible = visibleList(filter);

    while (!quit) {
      drawPackageList(visible, selected, scroll, filter, searchMode, message);

      struct tb_event ev;
      int rv = tb_poll_event(&ev);
      if (rv != TB_OK) {
        // 被 SIGWINCH 等信号打断属正常，回到循环头重绘即可
        if (rv == TB_ERR_POLL && tb_last_errno() == EINTR) continue;
        break;
      }
      if (ev.type == TB_EVENT_RESIZE) continue;
      if (ev.type != TB_EVENT_KEY) continue;

      // ---- 搜索输入模式 ----
      if (searchMode) {
        if (ev.ch >= 0x20 && ev.ch < 0x7f) {
          filter += static_cast<char>(ev.ch);
        } else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
          if (!filter.empty()) filter.pop_back();
        } else if (ev.key == TB_KEY_ENTER) {
          searchMode = false;
        } else if (ev.key == TB_KEY_ESC) {
          filter.clear();
          searchMode = false;
        }
        visible = visibleList(filter);
        selected = 0;
        scroll = 0;
        continue;
      }

      // ---- 普通模式按键 ----
      if (ev.key == TB_KEY_ESC || ev.key == TB_KEY_CTRL_C || ev.ch == 'q') {
        if (hasDirtyPackages()) {
          if (confirmQuit) {
            quit = true;
          } else {
            message = "Unsaved changes exist, press again to quit!";
            confirmQuit = true;
          }
        } else {
          quit = true;
        }
        continue;
      }
      confirmQuit = false;
      if (ev.key == TB_KEY_ARROW_DOWN || ev.ch == 'j') {
        if (!visible.empty() && selected + 1 < visible.size()) ++selected;
      } else if (ev.key == TB_KEY_ARROW_UP || ev.ch == 'k') {
        if (selected > 0) --selected;
      } else if (ev.key == TB_KEY_PGDN) {
        if (!visible.empty())
          selected = std::min(selected + 10, visible.size() - 1);
      } else if (ev.key == TB_KEY_PGUP) {
        selected = selected >= 10 ? selected - 10 : 0;
      } else if (ev.key == TB_KEY_HOME) {
        selected = 0;
      } else if (ev.key == TB_KEY_END) {
        if (!visible.empty()) selected = visible.size() - 1;
      } else if (ev.ch == '/') {
        searchMode = true;
        filter.clear();
        visible = visibleList(filter);
        selected = 0;
        scroll = 0;
      } else if (ev.ch == 'r') {
        visible = visibleList(filter);
        selected = 0;
        scroll = 0;
        message =
            "Refreshed (" + std::to_string(appList_.size()) + " packages)";
      } else if (ev.key == TB_KEY_ENTER) {
        if (!visible.empty()) {
          packageEditor(visible[selected]);
          visible = visibleList(filter);

          message = "Back to package list. (* = unsaved changes)";
        }
      } else if (ev.key == TB_KEY_CTRL_G) {
        fetchPackages();
        visible = visibleList(filter);
        selected = 0;
        scroll = 0;
        message =
            "Refreshed (" + std::to_string(appList_.size()) + " packages)";
      }
    }
    tb_shutdown();
  }

 private:
  std::map<std::string, CmdAppops> appList_;
  std::map<std::string, std::string> opDescriptions_;

  // 按关键字过滤包名（空过滤返回全部）
  std::vector<std::string> visibleList(const std::string& filter) const {
    std::vector<std::string> out;
    out.reserve(appList_.size());
    for (const auto& kv : appList_) {
      if (filter.empty() || kv.first.find(filter) != std::string::npos) {
        out.push_back(kv.first);
      }
    }
    return out;
  }

  bool hasDirtyPackages() const {
    for (const auto& kv : appList_) {
      if (kv.second.dirty()) {
        return true;
      }
    }

    return false;
  }

  std::string getDescription(const std::string& op) const {
    auto it = opDescriptions_.find(op);
    return (it != opDescriptions_.end()) ? it->second : "";
  }

  // 用指定属性填充一行剩余部分（反色标题栏 / 选中行高亮）
  static void fillRow(int y, int from, int w, uintattr_t fg, uintattr_t bg) {
    for (int x = from; x < w; ++x) tb_set_cell(x, y, ' ', fg, bg);
  }

  // 权限状态着色
  static uintattr_t modeColor(const std::string& mode) {
    if (mode == "allow") return TB_GREEN;
    if (mode == "foreground") return TB_CYAN;
    if (mode == "ignore") return TB_YELLOW;
    if (mode == "deny") return TB_RED;
    return TB_WHITE;  // default
  }

  // ---- 界面一：包列表 ----
  void drawPackageList(const std::vector<std::string>& visible, size_t selected,
                       int& scroll, const std::string& filter, bool searchMode,
                       const std::string& message) const {
    const int w = tb_width();
    const int h = tb_height();
    tb_clear();
    if (w <= 0 || h < 5) {
      tb_present();
      return;
    }

    // 标题栏
    std::string title = " AppOps Manager ";
    if (!filter.empty()) title += "[filter: " + filter + "] ";
    title += "(" + std::to_string(visible.size()) + "/" +
             std::to_string(appList_.size()) + ")";
    tb_printf(0, 0, TB_BLACK | TB_BOLD, TB_CYAN, "%s", title.c_str());
    fillRow(0, (int)title.size(), w, TB_BLACK | TB_BOLD, TB_CYAN);

    // 包列表（y=2 .. h-3）
    const int listTop = 2;
    const int listBottom = h - 3;
    const int viewRows = listBottom - listTop + 1;
    if (viewRows > 0) {
      // 保证选中项始终可见
      if (selected < (size_t)scroll) scroll = (int)selected;
      if (selected >= (size_t)scroll + viewRows)
        scroll = (int)selected - viewRows + 1;

      for (int row = 0; row < viewRows; ++row) {
        size_t idx = (size_t)(scroll + row);
        if (idx >= visible.size()) break;
        const bool isSel = (idx == selected);
        const CmdAppops& app = appList_.at(visible[idx]);

        std::string line = isSel ? "> " : "  ";
        line += app.dirty() ? "* " : "  ";
        line += visible[idx];

        uintattr_t fg = isSel ? TB_BLACK | TB_BOLD : TB_WHITE;
        uintattr_t bg = isSel ? TB_CYAN : TB_DEFAULT;
        tb_printf(0, listTop + row, fg, bg, "%s", line.c_str());
        if (isSel) fillRow(listTop + row, (int)line.size(), w, fg, bg);
      }
    }

    // 帮助栏
    const std::string help =
        " Up/Down: move  Enter: edit  /: search  r: refresh  Ctrl+G: give up "
        "all  q: quit ";
    tb_printf(0, h - 2, TB_BLACK, TB_WHITE, "%s", help.c_str());
    fillRow(h - 2, (int)help.size(), w, TB_BLACK, TB_WHITE);

    // 状态 / 搜索行
    if (searchMode) {
      std::string prompt = " Search: " + filter;
      tb_printf(0, h - 1, TB_BLACK | TB_BOLD, TB_YELLOW, "%s", prompt.c_str());
      fillRow(h - 1, (int)prompt.size(), w, TB_BLACK | TB_BOLD, TB_YELLOW);
    } else {
      tb_printf(0, h - 1, TB_YELLOW | TB_BOLD, TB_DEFAULT, " %s",
                message.c_str());
    }
    tb_present();
  }

  // ---- 界面二：单个包的 AppOps 编辑 ----
  void packageEditor(const std::string& pkg) {
    auto it = appList_.find(pkg);
    if (it == appList_.end()) return;
    CmdAppops& app = it->second;

    std::string message = "Ready";
    if (!app.loaded()) {  // 首次进入才真正查询系统
      message = app.fetchStatus() ? "Loaded appops from system"
                                  : "No appops records for this package";
    }

    bool quit = false, showDetail = false, confirmQuit = false;
    size_t selected = 0;
    int scroll = 0;

    while (!quit) {
      // 拍一份快照用于绘制（op 数通常只有几十个，开销可忽略）
      const std::vector<std::pair<std::string, std::string>> items(
          app.permissions().begin(), app.permissions().end());

      drawAppopsEditor(pkg, items, app, selected, scroll, showDetail, message);

      struct tb_event ev;
      int rv = tb_poll_event(&ev);
      if (rv != TB_OK) {
        if (rv == TB_ERR_POLL && tb_last_errno() == EINTR) continue;
        break;
      }
      if (ev.type != TB_EVENT_KEY) continue;

      if (ev.key == TB_KEY_ESC || ev.key == TB_KEY_CTRL_C || ev.ch == 'q') {
        if (app.dirty()) {
          if (confirmQuit) {
            quit = true;
          } else {
            message = "Unsaved changes, press again to quit!";
            confirmQuit = true;
          }
        } else {
          quit = true;
        }
        continue;
      }
      confirmQuit = false;
      if (ev.ch == 'b') {
        message = app.backup() ? "Backup written to " + getBackupPath(pkg)
                               : "Backup failed";
      } else if (ev.ch == 'o') {
        message = app.restore() ? "Restored from backup" : "Restore failed";
      } else if (items.empty()) {
        continue;
      } else if (ev.key == TB_KEY_ARROW_DOWN || ev.ch == 'j') {
        if (selected + 1 < items.size()) ++selected;
      } else if (ev.key == TB_KEY_ARROW_UP || ev.ch == 'k') {
        if (selected > 0) --selected;
      } else if (ev.key == TB_KEY_PGDN) {
        selected = std::min(selected + 10, items.size() - 1);
      } else if (ev.key == TB_KEY_PGUP) {
        selected = selected >= 10 ? selected - 10 : 0;
      } else if (ev.key == TB_KEY_ENTER || ev.ch == ' ') {
        app.cyclePermission(items[selected].first);
      } else if (ev.key == TB_KEY_CTRL_U) {
        showDetail = !showDetail;
      } else if (ev.ch == 's' && app.dirty()) {
        bool status = app.applyChange();
        if (status) {
          message = "Saved changes to system";
        } else {
          message = app.undoChange()
                        ? "Failed to apply changes, nothing changed!"
                        : "Failed to apply changes, and undo failed too! Will "
                          "backup origin and force reload!";
          app.backup();
          app.fetchStatus();
        }
      }
    }
  }

  void drawAppopsEditor(
      const std::string& pkg,
      const std::vector<std::pair<std::string, std::string>>& items,
      const CmdAppops& app, size_t selected, int& scroll, bool showDetail,
      const std::string& message) const {
    const int w = tb_width();
    const int h = tb_height();
    tb_clear();
    if (w <= 0 || h < 5) {
      tb_present();
      return;
    }

    // 标题栏
    std::string title = " AppOps: " + pkg + " ";
    if (app.dirty()) title += "(unsaved) ";
    tb_printf(0, 0, TB_BLACK | TB_BOLD, TB_MAGENTA, "%s", title.c_str());
    fillRow(0, (int)title.size(), w, TB_BLACK | TB_BOLD, TB_MAGENTA);

    // 权限列表(y=2)
    const int listTop = 2;
    const int descLines = showDetail ? 2 : 1;
    const int descY = h - 2 - descLines;
    const int listBottom = descY - 1;
    const int viewRows = listBottom - listTop + 1;

    if (!items.empty() && viewRows > 0) {
      if (selected < (size_t)scroll) scroll = (int)selected;
      if (selected >= (size_t)scroll + viewRows)
        scroll = (int)selected - viewRows + 1;

      const size_t kOpCol = 36;  // 操作名列宽
      for (int row = 0; row < viewRows; ++row) {
        size_t idx = (size_t)(scroll + row);
        if (idx >= items.size()) break;
        const auto& item = items[idx];
        const bool isSel = (idx == selected);
        const bool changed = app.isModified(item.first);

        std::string op = item.first;
        if (op.size() > kOpCol) op = op.substr(0, kOpCol);

        uintattr_t fg = isSel ? TB_BLACK | TB_BOLD : TB_WHITE;
        uintattr_t bg = isSel ? TB_MAGENTA : TB_DEFAULT;
        const int y = listTop + row;
        int col = 0;

        // 选择标记 + 操作名
        std::string prefix = (isSel ? "> " : "  ") + op +
                             std::string(kOpCol - op.size() + 2, ' ');
        tb_printf(col, y, fg, bg, "%s", prefix.c_str());
        col += (int)prefix.size();

        // 状态（按状态着色）
        uintattr_t modeFg =
            isSel ? (uintattr_t)(TB_BLACK | TB_BOLD)
                  : (uintattr_t)(modeColor(item.second) | TB_BOLD);
        tb_printf(col, y, modeFg, bg, "%s", item.second.c_str());
        col += (int)item.second.size();

        // 修改标记
        if (changed) {
          tb_printf(col, y, TB_RED | TB_BOLD, bg, " *");
          col += 2;
        }

        if (isSel) fillRow(y, col, w, fg, bg);
      }
    } else if (items.empty()) {
      tb_printf(0, listTop, TB_RED | TB_BOLD, TB_DEFAULT,
                " This package has no appops records. (ESC to go back)");
    }

    if (!items.empty() && selected < items.size()) {
      std::string op = items[selected].first;
      std::string desc = getDescription(op);
      int maxDescLen = std::max(0, w - 4);
      if (!desc.empty()) {
        if (showDetail) {
          std::string line1 = desc.substr(0, maxDescLen);
          tb_printf(1, descY, TB_BLUE, TB_DEFAULT, "%s", line1.c_str());
          if (desc.size() > maxDescLen) {
            std::string line2 = desc.substr(maxDescLen);
            tb_printf(1, descY + 1, TB_BLUE, TB_DEFAULT, "%s", line2.c_str());
          }
        } else {
          if (desc.size() > maxDescLen) {
            if (maxDescLen >= 3) {
              desc = desc.substr(0, maxDescLen - 3) + "...";
            } else {
              desc = desc.substr(0, maxDescLen);
            }
          }
          tb_printf(1, descY, TB_BLUE, TB_DEFAULT, "%s", desc.c_str());
        }
      } else {
        if (showDetail) {
          tb_printf(1, descY, TB_BLUE | TB_BOLD, TB_DEFAULT,
                    "(no description)");
          tb_printf(1, descY + 1, TB_BLUE | TB_BOLD, TB_DEFAULT,
                    "Everything is true...");
        } else {
          tb_printf(1, descY, TB_BLUE | TB_BOLD, TB_DEFAULT,
                    "(no description)");
        }
      }
    }

    // 帮助栏
    const std::string help =
        " Up/Down: move  Space/Enter: cycle  s: save  b: backup  "
        "o: restore  ESC: back  Ctrl+U: Detail";
    tb_printf(0, h - 2, TB_BLACK, TB_WHITE, "%s", help.c_str());
    fillRow(h - 2, (int)help.size(), w, TB_BLACK, TB_WHITE);

    // 消息栏
    tb_printf(0, h - 1, TB_YELLOW | TB_BOLD, TB_DEFAULT, " %s",
              message.c_str());
    tb_present();
  }
};

#endif
