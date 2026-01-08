#include "resources.h"

String StringDatabase::GetWiFissid() {
  return "Wiener Linien Monitor";
}

String StringDatabase::GetRBLPrompt() {
  return String(RBLPrompt);
}

String StringDatabase::GetEVAPrompt() {
  return String(EVAPrompt);
}

String StringDatabase::GetRBLFilterPrompt() {
  return String(RBLFilterPrompt);
}

String StringDatabase::GetEVAFilterPrompt() {
  return String(EVAFilterPrompt);
}

String StringDatabase::GetPowerModePrompt() {
  return String(EcoPrompt);
}

String StringDatabase::GetLineCountPrompt(int min, int max, int def) {
  String range = StringDatabase::GetFormatRange(min, max);
  String result =
    "How many shown depatures do you want to show at the same time on monitor "
    + range + "? (Default: " + String(def) + "). <br>"
    "Example: \"" + String(def) + "\".<br><br>"
    "<b>Lines to show:</b>";
  return result;
}

String StringDatabase::GetFormatRange(int min, int max) {
  if (min > max) {
    // Swap min and max if min is greater than max
    int temp = min;
    min = max;
    max = temp;
  }

  String result = "";

  if (min == max) {
    result += String(min);
  } else if (max - min == 1) {
    result += String(min) + " or " + String(max);
  } else {
    for (int i = min; i < max; i++) {
      result += String(i) + ", ";
    }
    result += "or " + String(max);
  }

  return result;
}