#include <SDCardManager.h>

#include <cstring>

#include "config.h"
#include "network/WifiCredentialStore.h"
#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("WifiCredentialOrderTest");
  auto& store = papyrix::WIFI_STORE;
  SdMan.reset();
  store.clearAll();

  runner.expectTrue(store.addCredential("First", "one"), "Save first network without connecting");
  runner.expectTrue(store.addCredential("Second", "two"), "Save second network without connecting");
  runner.expectTrue(store.addCredential("Third", "three"), "Save third network without connecting");

  runner.expectTrue(store.moveCredential("Third", -1), "Manually move saved network up");
  runner.expectTrue(strcmp(store.getCredentials()[1].ssid, "Third") == 0, "Moved network comes before Second");
  runner.expectTrue(store.moveCredential("First", 1), "Manually move saved network down");
  runner.expectTrue(strcmp(store.getCredentials()[0].ssid, "Third") == 0,
                    "Recent tries manually selected network first");

  runner.expectTrue(store.promoteCredential("Second"), "Successful connection promotes network");
  runner.expectTrue(strcmp(store.getCredentials()[0].ssid, "Second") == 0, "Connected network becomes Recent");
  runner.expectTrue(store.loadFromFile(), "Stored order reloads from SD");
  runner.expectTrue(strcmp(store.getCredentials()[0].ssid, "Second") == 0, "Recent order survives restart");
  runner.expectTrue(strcmp(store.getCredentials()[1].ssid, "Third") == 0, "Manual order survives restart");
  runner.expectFalse(store.moveCredential("Second", -1), "Cannot move first network past start");
  runner.expectFalse(store.moveCredential("Unknown", 1), "Cannot move missing network");
  runner.expectTrue(store.updateCredential("Third", "Renamed", "updated"), "Edit saved SSID and password offline");
  runner.expectTrue(store.findCredential("Third") == nullptr, "Old SSID is removed after edit");
  runner.expectTrue(strcmp(store.getCredentials()[1].ssid, "Renamed") == 0, "Edited network keeps its priority");
  runner.expectTrue(strcmp(store.findCredential("Renamed")->password, "updated") == 0, "Edited password persists");
  runner.expectFalse(store.updateCredential("Renamed", "Second", "overwrite"),
                     "Cannot overwrite another saved network");
  runner.expectTrue(store.loadFromFile(), "Edited network reloads from SD");
  runner.expectTrue(strcmp(store.findCredential("Renamed")->password, "updated") == 0,
                    "Edited password survives restart");
  runner.expectTrue(store.removeCredential("Renamed"), "Forget selected network");
  runner.expectTrue(store.findCredential("Renamed") == nullptr, "Forgotten network is not offered for connection");
  runner.expectFalse(store.addCredential("", "secret"), "Reject empty SSID");
  runner.expectFalse(store.addCredential("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", "secret"),
                     "Reject SSIDs longer than 32 bytes");
  SdMan.setSyncResult(false);
  runner.expectFalse(store.moveCredential("Second", 1), "Reject priority change when SD sync fails");
  runner.expectTrue(strcmp(store.getCredentials()[0].ssid, "Second") == 0, "Failed reorder keeps in-memory priority");
  SdMan.setSyncResult(true);
  runner.expectTrue(store.loadFromFile(), "Read saved network order after failed write");
  runner.expectTrue(strcmp(store.getCredentials()[0].ssid, "Second") == 0, "Failed reorder keeps stored priority");
  SdMan.setRenameResult(false);
  runner.expectFalse(store.removeCredential("First"), "Reject forget when SD rename fails");
  runner.expectTrue(store.findCredential("First") != nullptr, "Failed forget keeps in-memory credential");
  SdMan.setRenameResult(true);
  runner.expectTrue(store.loadFromFile(), "Failed forget keeps credential file readable");
  runner.expectTrue(store.findCredential("First") != nullptr, "Failed forget does not erase stored password");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
