#ifndef APPOPS_DATA
#define APPOPS_DATA

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <utility>

#include "appops_cmdline.h"
#include "appops_path.h"

class CmdAppops {
 public:
  explicit CmdAppops(const std::string& pkg) : packName_(pkg) {}

  // ---- 数据访问接口（供 TUI 层使用）----
  const std::string& packageName() const { return packName_; }
  const std::map<std::string, std::string>& permissions() const {
    return permissions_;
  }
  bool loaded() const { return loaded_; }
  // 是否存在未写回系统的修改
  bool dirty() const { return permissions_ != originalPermissions_; }
  // 判断某个 op 相对系统状态是否被修改过（界面标记 * 用）
  bool isModified(const std::string& op) const {
    auto cur = permissions_.find(op);
    if (cur == permissions_.end()) return false;
    auto orig = originalPermissions_.find(op);
    return orig == originalPermissions_.end() || orig->second != cur->second;
  }

  // 从系统读取当前 AppOps 状态
  bool fetchStatus() {
    permissions_ = getStatus(packName_);
    if (permissions_.empty()) {
      return false;
    }

    originalPermissions_ = permissions_;
    loaded_ = true;
    return true;
  }

  // 非 TUI 场景：把当前状态打印到标准输出
  void showStatus() const {
    std::printf("AppOps for %s:\n", packName_.c_str());
    if (permissions_.empty()) {
      std::printf("  (empty)\n");
      return;
    }
    for (const auto& kv : permissions_) {
      std::printf("  %-32s %s\n", kv.first.c_str(), kv.second.c_str());
    }
  }

  // 循环切换某项权限状态：default -> allow -> foreground -> ignore -> deny ->
  // default
  bool cyclePermission(const std::string& op) {
    auto it = permissions_.find(op);
    if (it == permissions_.end()) return false;
    it->second = nextMode(it->second);
    return true;
  }

  // 将缓存中的修改写回系统（只修改发生变化的项）
  bool applyChange() {
    for (const auto& kv : permissions_) {
      auto it = originalPermissions_.find(kv.first);

      if (it != originalPermissions_.end() && it->second == kv.second) {
        continue;
      }

      if (!isValidAppOpsMode(kv.second)) {
        return false;
      }

      std::string cmd = "cmd appops set " + shellQuote(packName_) + " " +
                        shellQuote(kv.first) + " " + shellQuote(kv.second);

      auto ret = execCommand(cmd);

      if (ret.first != 0) {
        return false;
      }
    }

    originalPermissions_ = permissions_;
    return true;
  }

  bool undoChange() {
    for (const auto& kv : originalPermissions_) {
      std::string cmd = "cmd appops set " + shellQuote(packName_) + " " +
                        shellQuote(kv.first) + " " + shellQuote(kv.second);
      auto ret = execCommand(cmd);

      if (ret.first != 0) {
        return false;
      }
    }

    return true;
  }

  bool backup() {
    if (originalPermissions_.empty()) {
      return false;
    }

    std::string dir = getDefaultBackupDir();

    if (!makeDirs(dir)) {
      return false;
    }

    std::string path = getBackupPath(packName_);

    std::ofstream backupFile;
    backupFile.open(path, std::ios::out | std::ios::trunc);
    if (!backupFile.is_open()) {
      return false;
    }
    for (const auto& line : originalPermissions_) {
      backupFile << line.first << ": " << line.second << "\n";
    }
    backupFile.close();
    return true;
  }

  bool restore() {
    std::string path = getBackupPath(packName_);

    std::ifstream backupFile;
    backupFile.open(path, std::ios::in);

    if (!backupFile.is_open()) {
      return false;
    }

    std::map<std::string, std::string> bperms;
    std::string line;

    while (std::getline(backupFile, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      auto pos = line.find(": ");
      if (pos == std::string::npos) {
        continue;
      }

      std::string op = line.substr(0, pos);
      std::string mode = line.substr(pos + 2);

      if (op.empty() || mode.empty()) {
        continue;
      }

      if (!isValidAppOpsMode(mode)) {
        continue;
      }

      bperms[op] = mode;
    }

    if (bperms.empty()) {
      return false;
    }

    permissions_ = bperms;
    loaded_ = true;

    return applyChange();
  }

 private:
  std::string packName_;
  std::map<std::string, std::string> permissions_;
  std::map<std::string, std::string> originalPermissions_;
  bool loaded_ = false;

  // 切换循环
  static std::string nextMode(const std::string& current) {
    if (current == "default") return "allow";
    if (current == "allow") return "foreground";
    if (current == "foreground") return "ignore";
    if (current == "ignore") return "deny";
    return "default";  // deny -> default
  }
};

#endif
