#define TB_IMPL
#include "appops_tui.h"

int main() {
  PermissionManager mg;
  mg.loadDescriptions("/storage/emulated/0/AppOps/AppOpsDesc");
  mg.fetchPackages();
  mg.showPackages();
}
