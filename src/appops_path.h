#ifndef APPOPS_PATH
#define APPOPS_PATH

#include <sys/stat.h>
#include <sys/types.h>

#include <cctype>
#include <cerrno>
#include <string>

inline std::string getDefaultBackupDir() {
  return "/storage/emulated/0/AppOps/.appops-backup";
}

inline bool makeDirs(const std::string& path) {
  if (path.empty()) {
    return false;
  }

  size_t pos = 0;

  while ((pos = path.find('/', pos + 1)) != std::string::npos) {
    std::string sub = path.substr(0, pos);

    if (sub.empty() || sub == "/") {
      continue;
    }

    if (::mkdir(sub.c_str(), 0700) != 0 && errno != EEXIST) {
      return false;
    }
  }

  if (::mkdir(path.c_str(), 0700) != 0 && errno != EEXIST) {
    return false;
  }

  return true;
}

inline std::string safeFileName(const std::string& name) {
  std::string result;
  result.reserve(name.size());

  for (char c : name) {
    unsigned char uc = static_cast<unsigned char>(c);

    if (std::isalnum(uc) || c == '.' || c == '-' || c == '_') {
      result.push_back(c);
    } else {
      result.push_back('_');
    }
  }

  return result;
}

inline std::string getBackupPath(const std::string& packageName) {
  return getDefaultBackupDir() + "/" + safeFileName(packageName) + ".appops";
}

#endif
