#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <WString.h>

/**
 * @struct StringDatabase
 *
 * @brief A utility struct for managing string prompts and instructions.
 */
struct StringDatabase {
private:
   /**
     * @brief The prompt for finding RBL/Stop ID.
     */
   const static constexpr char* RBLPrompt =
   "Wiener Linien RBL:"
    "<br>Find your RBL on <a href='https://till.mabe.at/rbl/' target='_blank' title='RBL Search'>https://till.mabe.at/rbl/</a>. Multiple RBLs can be combined by comma-separating them:"
    "<br>Example Single: \"49\""
    "<br>Example Multiple: \"1354,4202\".<br><br><b>RBL:</b>";

    const static constexpr char* EVAPrompt =
    "OEBB EVA:"
    "<br>Find your EVA/IBNR on <a href='https://www.michaeldittrich.de/ibnr/online.php' target='_blank' title='EVA/IBNR Search'>https://www.michaeldittrich.de/ibnr/online.php</a>."
    "<br>Example: \"810027\""
    "<br><br><b>EVA:</b>";

   /**
     * @brief The prompt for filtering lines.
     */
   const static constexpr char* RBLFilterPrompt =
    "<i>Optional.</i>"
    "Filter the lines to show by comma-separating the line numbers."
    "If empty, all directions will be shown.<br>"
    "Example: \"D,2,U2Z,43\".<br><br>"
    "<b>Filter RBL:</b>";

   const static constexpr char* EVAFilterPrompt =
    "<i>Optional.</i>"
    "Filter the lines to show by comma-separating the line numbers."
    "If empty, all directions will be shown.<br>"
    "Example: \"S45,S3\".<br><br>"
    "<b>Filter EVA:</b>";

   const static constexpr char* EcoPrompt = 
      "Select the Power Saving Mode. Can be enabled manually via short click on reset button or automatically if no data is received for 5 minutes (a stop with no nightline)."
      "<br>&nbsp;&nbsp;1=Light [Display Off]"
      "<br>&nbsp;&nbsp;2=Medium [Disply Off + CPU Limit]"
      "<br>&nbsp;&nbsp;3=Heavy [Disply Off + CPU Limit + WiFi Off]"
      "<br>Example: \"1\".<br><br><b>Power Save Mode:</b>";

  public:
  /**
     * @brief Get the Wi-Fi SSID string.
     * @return The Wi-Fi SSID string.
     */
  static String GetWiFissid();

  /**
     * @brief Get the RBL/Stop ID prompt.
     * @return The RBL/Stop ID prompt.
     */
  static String GetRBLPrompt();

  static String GetEVAPrompt();

  /**
     * @brief Get the line filter prompt.
     * @return The line filter prompt.
     */
   static String GetRBLFilterPrompt();

   static String GetEVAFilterPrompt();
   
   static String GetPowerModePrompt();

   /**
     * @brief Get the prompt for specifying the number of lines to show.
     * @param min The minimum number of lines.
     * @param max The maximum number of lines.
     * @param def The default number of lines.
     * @return The prompt for specifying the number of lines to show.
     */
  static String GetLineCountPrompt(int min, int max, int def);

private:
  /**
     * @brief Format a range of values as a string.
     * @param min The minimum value.
     * @param max The maximum value.
     * @return The formatted range string.
     *
     * Example: GetFormatRange(1, 3) returns "1, 2, or 3".
     */
  static String GetFormatRange(int min, int max);
};

#endif // __STRINGS_H__