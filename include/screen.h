#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <map>
#include <vector>
#include <TFT_eSPI.h>  // by Bodmer 2.5.43, user config 206

#include "config.h"

extern Configuration config;

struct ScreenEntity {
  String right_txt;  // Line name 2, 72, D etc.
  String left_txt;
  std::vector<String> lines;
  bool is_barrier_free;
  bool has_folding_ramp;
  bool is_airport;
};

enum ScrollState { WAIT_BEFORE, SCROLLING, WAIT_AFTER };

/**
 * @brief The `Screen` class represents a screen with multiple rows, each
 * displaying text and countdown information.
 *
 * This class is designed for use with TFT displays and provides methods for
 * setting and drawing text on each idx_row, as well as managing scrolling text
 * and drawing separator lines.
 */
class Screen {
    public:

        static Screen& getInstance();

        // Delete copy constructor and assignment operator
        Screen(const Screen&) = delete;
        void operator=(const Screen&) = delete;

        BaseType_t acquire();

        void release();

       /**
       * @brief Constructor to initialize a `Screen` object with the specified
       * number of rows and lines per idx_row.
       *
       * @param cnt_rows The number of rows on the screen.
       * @param number_text_lines The number of lines of text that can be displayed
       * in each idx_row.
       */
       void SetRowCount(int cnt_rows);

       void SetRows(const std::vector<ScreenEntity>& vec_screen_entity);

       /**
       * @brief Sets the content for a specific idx_row on the screen and updates
       * its display.
       *
       * @param monitor The `Monitor` object containing the text and countdown
       * information for the idx_row.
       * @param idx_row The idx_row index to set the content for.
       */
       void DrawRow(const ScreenEntity& monitor, int idx_row);

       /**
       * @brief Converts a single German string to Latin alphabet.
       *
       * This function takes a single German string and converts it from German
       * characters (e.g., ä, Ä, ö, Ö, ü, Ü, ß) to their Latin alphabet equivalents
       * (a, A, o, O, u, U, s).
       *
       * @param input The German string to be converted.
       *
       * @return The converted string in Latin alphabet.
       */
       static String ConvertGermanToLatin(String input);

       int GetNumberRows();

       void DrawCenteredText(const String& text);

       void clear();

    private:
        explicit Screen(TFT_eSPI& tft, int cnt_rows);

        /**
       * @brief Draws the countdown text on the screen for a specific idx_row.
       *
       * @param countdown The countdown text to be displayed.
       * @param idx_row The idx_row index where the countdown text should be drawn.
       * @param p_font The pointer to the p_font to be used for rendering the text.
       */
        void DrawCountdown(const String& countdown, int idx_row, const GFXfont* pFont) const;

        /**
       * @brief Draws the name text on the screen for a specific idx_row.
       *
       * @param name The name text to be displayed.
       * @param idx_row The idx_row index where the name text should be drawn.
       * @param p_font The pointer to the p_font to be used for rendering the text.
       */
        void DrawName(const String& name, int row, const GFXfont* pFont) const;

        /**
       * @brief Sets and draws the middle lines and text content for a specific
       * idx_row on the screen.
       *
       * @param vec_text_lines The vector of text lines to be displayed in the idx_row.
       * @param vec_icon_wheelchair The vector of wheelchairs to be displayed in the idx_row.
       * @param idx_row The idx_row index where the text content should be set and
       * drawn.
       */
        void DrawMiddleText(const std::vector<String>& vec_text_lines, bool is_barrier_free,  bool has_folding_ramp, bool is_airport, int idx_row);
        /**
       * @brief Draws text on a sprite and pushes it to the screen at the specified
       * coordinates.
       *
       * @param text The text to be drawn on the sprite.
       * @param idx_row The idx_row index where the text should be displayed.
       * @param x The X-coordinate where the text should be drawn.
       * @param y The Y-coordinate where the text should be drawn.
       * @param p_font The pointer to the p_font to be used for rendering the text.
       */
        void DrawTextOnSprite(const String& text, int idx_row, int x, int y, const GFXfont* p_font, int px_max_width) const;

        /**
       * @brief Draws separator lines between rows on the screen.
       */
        void drawLines() const;

        static inline void drawThickLine(TFT_eSprite &s, int x0, int y0, int x1, int y1, int thickness, uint16_t color);

        void drawWheelchairIcon(TFT_eSprite &s, int x, int y, int size, uint16_t color, bool has_underline);

        /**
       * @brief Sets the maximum px_width of the name text in pixels.
       *
       * @param px_w Width of the name text in pixels to compare with the current
       * maximum.
       */
        void SetMaxNameTextWidth_px(int px_w);

        /**
       * @brief Sets the maximum px_width of the countdown text in pixels.
       *
       * @param px_w The px_width of the countdown text in pixels to compare with
       * the current maximum.
       */
        void SetMaxCountdownTextWidth_px(int px_w);

        /**
       * @brief Gets the maximum px_width of the countdown text in pixels.
       *
       * @return The maximum px_width of the countdown text in pixels.
       */
        int GetMaxCountdownTextWidth_px() const;

        /**
       * @brief Gets the maximum px_width of the name text in pixels.
       *
       * @return The maximum px_width of the name text in pixels.
       */
        int GetMaxNameTextWidth_px() const;

        /**
       * @brief Calculates the coordinate position of a line based on the specified
       * parameters.
       *
       * @param bodyLength The total length of the body where lines are positioned.
       * @param segmentMargin The margin between segments.
       * @param segmentLength The length of each segment.
       * @param segmentCount The total number of segments.
       * @param n The index of the line.
       *
       * @return The Coordinate position of the line.
       */
        int CalculateLineDistance_px(double bodyLength, double segmentMargin, double segmentLength, int segmentCount, int n) const;

        /**
       * @brief Calculates the px_width of a text string using the specified p_font.
       *
       * @param p_font The p_font used for rendering the text.
       * @param str The text string to calculate the px_width for.
       *
       * @return The px_width of the text string in pixels.
       */
        int CalculateFontWidth_px(const GFXfont* p_font, const String& str) const;

        /**
       * @brief Calculates the height of the text when rendered using the specified
       * p_font.
       *
       * This function creates a temporary sprite, sets the provided p_font, and
       * then retrieves the p_font height.
       *
       * @param p_font The p_font used for rendering the text.
       *
       * @return The height of the text when rendered with the specified p_font in
       * pixels.
       */
        int CalculatefontHeight_px(const GFXfont* p_font) const;

        void SetMinTextSprite_px(int px_min);
     
        TFT_eSPI& _tft;
        SemaphoreHandle_t internal_mutex;
        uint16_t color_bg;
        uint16_t color_txt;
        ///< The maximum px_width of the name text in pixels.
        int px_max_width_name_text;
        ///< The maximum px_width of the countdown text in pixels.
        int px_max_width_countdown_text;
        ///< The px_width bewwen tables.
        int px_separate_line_height;
        ///< The px_width bewwen tables.
        int px_margin_lines;
        ///< The number of rows on the screen.
        int cnt_rows;
        ///< The number of lines of text that can be displayed in each idx_row.
        int number_text_lines;
        ///< The margin value for text and components.
        const int px_margin;
        ///< Coordinates for text scrolling in each idx_row.
        std::vector<std::vector<int>> vec_scrolls_coords;
        std::vector<std::vector<bool>> vec_init_scrolls_coords;
        std::vector<std::vector<ScrollState>> scroll_states;
        std::vector<std::vector<uint32_t>> scroll_timestamps;
        int px_min_text_sprite;
        mutable std::map<const GFXfont*, int> font_height_cache;

    public:
        void FullResetScroll();

        void SelectiveResetScroll(const std::vector<bool>& isNeedReset);
};

#endif // __SCREEN_H_