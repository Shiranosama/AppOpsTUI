#ifndef APPOPS_CMDLINE
#define APPOPS_CMDLINE

#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

// 命令执行
inline std::string shellQuote(const std::string& s) {
  std::string quoted;
  quoted.reserve(s.size() + 2);
  quoted.push_back('\'');

  for (char c : s) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(c);
    }
  }

  quoted.push_back('\'');
  return quoted;
}

inline bool isValidAppOpsMode(const std::string& mode) {
  return mode == "default" || mode == "allow" || mode == "foreground" ||
         mode == "ignore" || mode == "deny";
}

inline std::pair<int, std::string> execCommand(const std::string& cmd) {
  std::string output{};
  FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
  if (!pipe) {
    return {-1, ""};
  }

  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  int status = pclose(pipe);
  if (status == -1) {
    return {-1, output};
  }

  if (WIFEXITED(status)) {
    return {WEXITSTATUS(status), output};
  }

  if (WIFSIGNALED(status)) {
    return {128 + WTERMSIG(status), output};
  }
  return {status, output};
}

// 获取所有包
inline std::vector<std::string> getApplist() {
  std::vector<std::string> appList{};
  auto ret = execCommand("pm list packages");

  if (ret.first != 0) {
    return appList;
  }

  static std::regex re(R"(package:(\S+))");
  const std::string& output = ret.second;

  for (std::sregex_iterator it(output.begin(), output.end(), re), end;
       it != end; ++it) {
    appList.push_back((*it)[1].str());
  }
  return appList;
}

inline std::map<std::string, std::string> getStatus(const std::string& pac) {
  std::map<std::string, std::string> perms{};
  auto ret = execCommand("cmd appops get " + shellQuote(pac));

  if (ret.first != 0) {
    return perms;
  }

  static std::regex re(
      R"(([\w.:]+):\s*(allow|foreground|ignore|deny|default))");
  const std::string& output = ret.second;

  for (std::sregex_iterator it(output.begin(), output.end(), re), end;
       it != end; ++it) {
    perms[(*it)[1].str()] = (*it)[2].str();
  }

  return perms;
}

#endif
