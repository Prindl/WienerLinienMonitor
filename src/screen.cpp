#include "colors.h"
#include "power_manager.h"
#include "screen.h"

Screen& Screen::getInstance(){
    PowerManager& pm = PowerManager::getInstance();
    static Screen instance(pm.get_tft(), config.get_number_lines());
    return instance;
}


Screen::Screen(TFT_eSPI& tft, int cnt_rows)
: _tft(tft),
  px_max_width_name_text(0),
  px_max_width_countdown_text(0),
  px_separate_line_height(3),
  px_margin_lines(2),
  number_text_lines(TEXT_ROWS_PER_MONITOR),
  px_margin(8),
  px_min_text_sprite(std::numeric_limits<int>::max()) {
    SetRowCount(cnt_rows);
    this->internal_mutex = xSemaphoreCreateMutex();
    if (this->internal_mutex == NULL) {
        Serial.println("[ScreenManager] Error: Could not create Mutex!");
    }
}

BaseType_t Screen::acquire() {
    return xSemaphoreTake(this->internal_mutex, portMAX_DELAY);
}

void Screen::release() {
    if (this->internal_mutex != NULL) {
        xSemaphoreGive(this->internal_mutex);
    }
}


void Screen::SetRowCount(int num_rows){
    if (cnt_rows != num_rows){
        clear();
        cnt_rows = num_rows;
        vec_scrolls_coords.resize(cnt_rows, std::vector<int>(number_text_lines, 0));
        vec_init_scrolls_coords.resize(cnt_rows, std::vector<bool>(number_text_lines, false));
        scroll_states.resize(cnt_rows, std::vector<ScrollState>(number_text_lines, ScrollState::WAIT_BEFORE));
        scroll_timestamps.resize(cnt_rows, std::vector<uint32_t>(number_text_lines, 0));
    }
}

void Screen::SetRows(const std::vector<ScreenEntity>& vec_screen_entity) {
    // Calculate the sizes before rendering and take the max of each row
    const GFXfont* p_font = &FreeSansBold24pt7b;
    std::vector<int> sizes_name;
    std::vector<int> sizes_countdown;
    for (size_t i = 0; i < vec_screen_entity.size(); ++i) {
        if (i > cnt_rows - 1) {
            return;
        }
        const ScreenEntity& monitor = vec_screen_entity[i];
        
        // Calculate max Sides text block size.
        sizes_name.push_back(CalculateFontWidth_px(p_font, monitor.right_txt));
        sizes_countdown.push_back(CalculateFontWidth_px(p_font, monitor.left_txt));
    }
    if (sizes_name.size() > 0) {
        SetMaxNameTextWidth_px(*std::max_element(sizes_name.begin(), sizes_name.end()));
    }
    if (sizes_countdown.size() > 0) {
        SetMaxCountdownTextWidth_px(*std::max_element(sizes_countdown.begin(), sizes_countdown.end()));
    }
        
    for (size_t i = 0; i < vec_screen_entity.size(); ++i) {
        DrawRow(vec_screen_entity[i], i);
    }
}

void Screen::DrawRow(const ScreenEntity& monitor, int idx_row) {
    if (idx_row > cnt_rows - 1) {
        return;
    }
    const GFXfont* p_font = &FreeSansBold24pt7b;
    // Draw Text;
    DrawName(monitor.right_txt, idx_row, p_font);
    DrawCountdown(monitor.left_txt, idx_row, p_font);
    DrawMiddleText(monitor.lines, monitor.is_barrier_free, monitor.has_folding_ramp, idx_row);

    // Draw Separet Lines
    drawLines();
}

bool Screen::IsEnoughSpaceForMiddleText(const String& str) {
    const GFXfont* p_font = &FreeSansBold12pt7b;
    return GetMinTextSprite_px() >= CalculateFontWidth_px(p_font, str);
}

void Screen::PrintCordDebug() {
    // Print the contents of the vectors
    for (int i = 0; i < vec_scrolls_coords.size(); i++) {
        Serial.print("[");
        for (int j = 0; j < vec_scrolls_coords[i].size(); j++) {
        Serial.print(vec_scrolls_coords[i][j]);
        if (j != vec_scrolls_coords[i].size() - 1) {
            Serial.print(", ");
        }
        }
        Serial.print("]");
    }

    for (int i = 0; i < vec_init_scrolls_coords.size(); i++) {
        Serial.print("[");
        for (int j = 0; j < vec_init_scrolls_coords[i].size(); j++) {
        Serial.print(vec_init_scrolls_coords[i][j]);
        if (j != vec_init_scrolls_coords[i].size() - 1) {
            Serial.print(", ");
        }
        }
        Serial.print("]");
    }
    Serial.println("");
// #endif
}

String Screen::ConvertGermanToLatin(String input) {
    String output;
    output.reserve(input.length());
    for (int i = 0; i < input.length(); ++i) {
        char c = input[i];
        // Handle multi-byte UTF-8 sequences
        if (c == 0xC3) {
            // UTF-8 German umlauts
            char next = input[i + 1];
            if (next == 0xA4) { output += "ae"; i++; } // ä
            else if (next == 0x84) { output += "AE"; i++; } // Ä
            else if (next == 0xB6) { output += "oe"; i++; } // ö
            else if (next == 0x96) { output += "OE"; i++; } // Ö
            else if (next == 0xBC) { output += "ue"; i++; } // ü
            else if (next == 0x9C) { output += "UE"; i++; } // Ü
            else if (next == 0x9F) { output += "ss"; i++; } // ß
            else { output += c; }
            continue;
        } else if(c == 0xC2) {
            char next = input[i + 1];
            if (next == 0xA0) { output += " "; i++; } // HTML Non Breaking Space
            else { output += c; }
            continue;
        }
        output += c;
    }
    return output;
}

int Screen::GetMinTextSprite_px() {
    return px_min_text_sprite;
}

int Screen::GetNumberRows(){
    return cnt_rows;
}

void Screen::DrawCenteredText(const String& text){
    const GFXfont* p_font = &FreeSansBold12pt7b;

    // Calculate dimensions
    auto& vec_scroll_cord = vec_scrolls_coords[0];
    auto& is_init_cords = vec_init_scrolls_coords[0];
    int px_height_font = CalculatefontHeight_px(p_font);
    int px_width = _tft.width() - px_margin * 2;

    SetMinTextSprite_px(px_width);

    // Create and configure sprite
    TFT_eSprite sprite(&_tft);
    sprite.createSprite(px_width, px_height_font);
    int px_full_string = CalculateFontWidth_px(p_font, text.c_str());

    // Reset to the right edge of the screen
    if (vec_scroll_cord[0] < (px_full_string * -1)) {
        vec_scroll_cord[0] = sprite.width();
    }
    if (px_full_string > px_width) {
        //scroll text 2 is mean px per frame
        vec_scroll_cord[0] -= config.settings.scrollrate;
        if (!is_init_cords[0] && text.length()) {
            // Scroll if text is bigger than available space
            vec_scroll_cord[0] = sprite.width();
            is_init_cords[0] = true;
        }
    } else if (px_full_string < vec_scroll_cord[0]) {
        // reset sscrolls cord
        vec_scroll_cord[0] = 0;
        is_init_cords[0] = false;
    }

    // Draw text on the sprite
    sprite.setTextColor(COLOR_TEXT_YELLOW);
    sprite.fillSprite(COLOR_BG);
    sprite.setFreeFont(p_font);
    sprite.drawString(text.c_str(), vec_scroll_cord[0], 0);

    // Push sprite to display
    int x_cord = px_margin;
    int y_cord = (_tft.height() - px_height_font) / 2;
    sprite.pushSprite(x_cord, y_cord);
    sprite.deleteSprite();

}

void Screen::clear() {
    _tft.fillScreen(COLOR_BG);
}

std::vector<String> Screen::ConvertGermanToLatin(const std::vector<String>& vecGerman) {
    std::vector<String> output;
    for (auto& str : vecGerman) {
        output.push_back(ConvertGermanToLatin(str));
    }
    return output;
}

void Screen::DrawCountdown(const String& countdown, int idx_row, const GFXfont* pFont) const {
    int px_font_width = CalculateFontWidth_px(pFont, countdown);

    int px_dx_countdown_margin = _tft.width() - px_font_width - px_margin;

    DrawTextOnSprite(countdown, idx_row, px_dx_countdown_margin, 0, pFont, GetMaxCountdownTextWidth_px());
}

void Screen::DrawName(const String& name, int row, const GFXfont* pFont) const {
    int dxNameMargin_px = px_margin;
    DrawTextOnSprite(name, row, dxNameMargin_px, 0, pFont, GetMaxNameTextWidth_px());
}

inline void Screen::drawThickLine(TFT_eSprite &s, int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
    // Simple thick line: draw many parallel offset lines (cheap and looks fine at small thickness)
    int half = thickness / 2;
    for (int ox = -half; ox <= half; ++ox) {
        for (int oy = -half; oy <= half; ++oy) {
            s.drawLine(x0 + ox, y0 + oy, x1 + ox, y1 + oy, color);
        }
    }
}

void Screen::drawWheelchairIcon(TFT_eSprite &s, int x, int y, int size, uint16_t color, bool has_underline) {
    // Reference design is 24x24 units
    const float R = size / 24.0f;
    auto X = [&](float vx){ return x + (int)roundf(vx * R); };
    auto Y = [&](float vy){ return y + (int)roundf(vy * R); };

    // thickness scaled (in pixels)
    int stroke = (int)max(1.0f, floorf(R * 0.9f)); // 1..3 typically

    // --- Head (filled circle) ---
    int hx = X(9.0f);
    int hy = Y(3.0f);
    int hr = max(1, (int)roundf(2.0f * R));
    s.fillCircle(hx, hy, hr, color);

    // --- Seat / backrest / frame (thick lines) ---
    // Back support: (9,6) down to (9,12)
    drawThickLine(s, X(9.0f), Y(6.0f), X(9.0f), Y(12.0f), stroke, color);

    // Top seat bar: (9,6) to (14,6)
    drawThickLine(s, X(9.0f), Y(7.0f), X(18.0f), Y(7.0f), stroke, color);

    // Seat front: (9,12) to (14,12)
    drawThickLine(s, X(9.0f), Y(12.0f), X(14.0f), Y(12.0f), stroke, color);

    // Frame diagonal / leg: (14,12) to (17,17)
    drawThickLine(s, X(14.0f), Y(12.0f), X(17.0f), Y(17.0f), stroke, color);

    // --- Wheel (thick ring) ---
    // Outer ring
    int wheel_cx = X(9.0f);
    int wheel_cy = Y(16.0f);
    int wheel_r = max(1, (int)roundf(6.0f * R));
    // Draw thicker ring by drawing several concentric circles
    for (int rr = 0; rr < stroke; ++rr) {
        s.drawCircle(wheel_cx, wheel_cy, wheel_r - rr, color);
    }
    // Hub (small filled circle)
    int hub_r = max(1, (int)roundf(2.0f * R));
    s.fillCircle(wheel_cx, wheel_cy, hub_r, color);

    // --- Foot / front support line (optional for realism) ---
    drawThickLine(s, X(17.0f), Y(17.0f), X(20.0f), Y(20.0f), stroke, color);
    if (has_underline){
        drawThickLine(s, X(3.0f), Y(23.0f), X(20.0f), Y(23.0f), stroke, color);
    }
}

void Screen::DrawMiddleText(const std::vector<String>& vec_text_lines, bool is_barrier_free, bool has_folding_ramp, int idx_row) {

    const GFXfont* p_font = &FreeSansBold12pt7b;

    // Calculate dimensions
    auto& vec_scroll_cord = vec_scrolls_coords[idx_row];
    auto& is_init_cords = vec_init_scrolls_coords[idx_row];
    auto& vec_scroll_state = scroll_states[idx_row];
    auto& vec_scroll_ts = scroll_timestamps[idx_row];
    int px_width_countdown = GetMaxCountdownTextWidth_px();
    int px_width_stopcode = GetMaxNameTextWidth_px();
    int px_height_font = CalculatefontHeight_px(p_font);
    // Minus two margins for name and two margins for countdown
    // 4 is count margin around Coundown and Stop code
    int px_width = _tft.width() - px_width_countdown - px_width_stopcode - px_margin * 4;
    if (px_width <= 0 || px_height_font <= 0) {
        Serial.println("Sprite dimensions invalid!");
        return;
    }

    SetMinTextSprite_px(px_width);


    // Create and configure sprite
    TFT_eSprite sprite(&_tft);
    sprite.createSprite(px_width, px_height_font);

    int cnt_lines_actual = static_cast<int>(vec_text_lines.size());
    for (int i = 0; i < std::min(number_text_lines, cnt_lines_actual); ++i) {
        bool draw_wheelchair = i == 0 && is_barrier_free;
        uint32_t now = millis();
        int px_height_full = static_cast<double>(_tft.height() / cnt_rows);
        int dy = CalculateLineDistance_px(px_height_full, px_margin_lines, px_height_font, number_text_lines, i);
        int px_full_text = CalculateFontWidth_px(p_font, vec_text_lines[i].c_str());
        int px_full_string = draw_wheelchair ? px_full_text + px_height_font : px_full_text;

        // Reset to the right edge of the screen
        int px_to_scroll = px_full_string - px_width;
        bool do_scrolling = px_to_scroll > 0;

        switch (vec_scroll_state[i]) {
            case WAIT_BEFORE:
                if (do_scrolling) {
                    if (vec_scroll_ts[i] == 0) {
                        vec_scroll_ts[i] = now;
                        vec_scroll_cord[i] = 0;//sprite.width()
                    }
                    if (now - vec_scroll_ts[i] >= config.settings.delay_scroll) {
                        vec_scroll_state[i] = SCROLLING;
                    }
                } else {
                    vec_scroll_cord[i] = 0;
                }
                break;

            case SCROLLING:
                if (do_scrolling) {
                    //scroll text 2 is mean px per frame
                    vec_scroll_cord[i] -= config.settings.scrollrate;
                    if (i > 0 && !is_init_cords[i] && vec_text_lines[i].length()) {
                        // Scroll if text is bigger than available space
                        vec_scroll_cord[i] = 0;//sprite.width();
                        is_init_cords[i] = true;
                    }
                } else if (px_full_text < vec_scroll_cord[i]) {
                    // reset sscrolls cord
                    vec_scroll_cord[i] = 0;
                    is_init_cords[i] = false;
                }
                // Scrolling Finished
                if (vec_scroll_cord[i] <= (px_to_scroll * -1)) {
                    vec_scroll_ts[i] = now;
                    vec_scroll_state[i] = WAIT_AFTER;
                }
                break;

            case WAIT_AFTER:
                if (do_scrolling) {
                    if (now - vec_scroll_ts[i] >= config.settings.delay_scroll) {
                        // reset scroll coordinates
                        vec_scroll_cord[i] = 0; //sprite.width()
                        vec_scroll_state[i] = WAIT_BEFORE;
                        vec_scroll_ts[i] = now;
                    }
                } else {
                    vec_scroll_cord[i] = 0;
                }
                break;
        }

        // Draw text on the sprite
        sprite.setTextColor(COLOR_TEXT_YELLOW);
        sprite.fillSprite(COLOR_BG);
        sprite.setFreeFont(p_font);
        sprite.drawString(vec_text_lines[i].c_str(), vec_scroll_cord[i], 0);

        if (draw_wheelchair) {
            drawWheelchairIcon(sprite, px_full_string - px_height_font + vec_scroll_cord[i], i*px_height_font, px_height_font, COLOR_TEXT_YELLOW, has_folding_ramp);
        }

        // Push sprite to display
        int x_cord = px_margin + px_margin + px_width_stopcode;
        int y_cord = dy + ((_tft.height() / cnt_rows) * idx_row);
        sprite.pushSprite(x_cord, y_cord);
    }
    sprite.deleteSprite();
}

void Screen::DrawTextOnSprite(const String& text, int idx_row, int x, int y, const GFXfont* p_font, int px_max_width) const {
    const int px_width_sprite = CalculateFontWidth_px(p_font, text);
    const int px_height_font = CalculatefontHeight_px(p_font);
    const int px_height_full = _tft.height() / cnt_rows;
    const int dy = CalculateLineDistance_px(px_height_full, 0, px_height_font, 1, 0);

    TFT_eSprite sprite(&_tft);
    TFT_eSprite bg_left_sprite(&_tft);
    TFT_eSprite bg_right_sprite(&_tft);
    // TFT_eSprite bg_sprite(&_tft);  // Separate sprite for squares

    // Set background colors based on DEBUG mode
    uint16_t color_left, color_middle, color_right;
    //#define DEBUG_DrawTextOnSprite
    #ifdef DEBUG_DrawTextOnSprite
    leftColor = TFT_GREEN;
    middleColor = TFT_RED;
    rightColor = TFT_BLUE;
    #else
    color_left = color_middle = color_right = COLOR_BG;
    #endif

    // Special symbols that draw manualy
    bool drawTopRightSquare = text == String("◱");
    bool drawBottomLeftSquare = text == String("◳");

    if (drawTopRightSquare || drawBottomLeftSquare) {
        sprite.createSprite(px_height_font, px_height_font);
        sprite.fillSprite(color_middle);
        int px_sqaure_size = (px_height_font / 2) - 6;
        if (drawTopRightSquare) {
        // down left
        sprite.fillRect(px_height_font / 2, 0, px_sqaure_size, px_sqaure_size,
                        TFT_YELLOW);
        // up right
        sprite.fillRect(0, px_height_font / 2, px_sqaure_size, px_sqaure_size,
                        TFT_YELLOW);
        } else if (drawBottomLeftSquare) {
        // left up
        sprite.fillRect(0, 0, px_sqaure_size, px_sqaure_size, TFT_YELLOW);
        // down right
        sprite.fillRect(px_height_font / 2, px_height_font / 2, px_sqaure_size,
                        px_sqaure_size, TFT_YELLOW);
        }

    } else {
        sprite.createSprite(px_width_sprite, px_height_font);
        sprite.fillSprite(color_middle);
        sprite.setTextColor(COLOR_TEXT_YELLOW);
        sprite.setFreeFont(p_font);
        sprite.drawString(text, 0, 0);
    }
    // if (px_width_sprite < px_max_width) {
    //  Create and display left background sprite
    bg_left_sprite.createSprite(px_max_width - px_width_sprite, px_height_font);
    bg_left_sprite.fillSprite(color_left);
    int bg_left_x_cord = x - (px_max_width - px_width_sprite);
    int bg_left_y_cord = y + dy + (px_height_full * idx_row);
    bg_left_sprite.pushSprite(bg_left_x_cord, bg_left_y_cord);
    bg_left_sprite.deleteSprite();

    // Create and display right background sprite
    const int bg_right_x = x + px_width_sprite;
    const int bg_right_width = px_max_width - px_width_sprite;
    bg_right_sprite.createSprite(bg_right_width, px_height_font);
    bg_right_sprite.fillSprite(color_right);
    bg_right_sprite.pushSprite(bg_right_x, y + dy + (px_height_full * idx_row));
    bg_right_sprite.deleteSprite();
    //}

    // Create and display text sprite
    /*sprite.createSprite(px_width_sprite, px_height_font);
            sprite.fillSprite(color_middle);
            sprite.setTextColor(COLOR_TEXT_YELLOW);
            sprite.setFreeFont(p_font);
            sprite.drawString(text, 0, 0);*/
    sprite.pushSprite(x, y + dy + (px_height_full * idx_row));
    sprite.deleteSprite();
}

void Screen::drawLines() const {
    // Create a TFT_eSprite for drawing separator lines
    TFT_eSprite lines_separator(&_tft);
    // Create a N-pixel tall sprite
    lines_separator.createSprite(_tft.width(), px_separate_line_height);

    for (int i = 1; i < cnt_rows; ++i) {
        int px_height_and_sprite = (_tft.height() - (px_separate_line_height * i));
        int y_cord = px_height_and_sprite / cnt_rows * i;
        // Clear the sprite and draw a horizontal line
        lines_separator.fillSprite(TFT_BLACK);
        lines_separator.drawFastHLine(0, 0, _tft.width(), TFT_BLACK);

        // Push the sprite to display at the specified Y-coordinate
        lines_separator.pushSprite(0, y_cord);
    }
    // Delete the sprite to free up memory
    lines_separator.deleteSprite();
}

void Screen::SetMaxNameTextWidth_px(int px_w) {
    if (px_w != px_max_width_name_text) {
        px_max_width_name_text = px_w;
        _tft.fillScreen(COLOR_BG);
    }
}

void Screen::SetMaxCountdownTextWidth_px(int px_w) {
    if (px_w > px_max_width_countdown_text) {
        px_max_width_countdown_text = px_w;
        _tft.fillScreen(COLOR_BG);
    }
}

int Screen::GetMaxCountdownTextWidth_px() const {
    return px_max_width_countdown_text;
}

int Screen::GetMaxNameTextWidth_px() const {
    return px_max_width_name_text;
}

int Screen::CalculateLineDistance_px(double bodyLength, double segmentMargin, double segmentLength, int segmentCount, int n) const {
    // Calculate the length occupied by segments
    double segmentsLength = segmentCount * segmentLength;

    // Calculate the length occupied by margins
    double marginsLength = (segmentCount - 1) * segmentMargin;

    // Add the lengths to get the total
    double totalSegmentsLength = segmentsLength + marginsLength;

    // Calculate the distance from the beginning of the body to the first line
    double x_firstLine = (bodyLength - totalSegmentsLength) / 2;

    // Calculate the final position based on the first line and segment spacing
    double finalPosition = x_firstLine + (n * (segmentLength + segmentMargin));

    // Convert the final position to an integer and return it
    return static_cast<int>(finalPosition);
}

int Screen::CalculateFontWidth_px(const GFXfont* p_font, const String& str) const {
    // This symbol not exist in p_font this squares draw manuany
    // In squeres Width == Height
    if (str == String("◱") || str == String("◳")) {
        return CalculatefontHeight_px(p_font);  // height == px_width in square
    }

    TFT_eSprite Calculator = TFT_eSprite(&_tft);
    Calculator.setFreeFont(p_font);
    int px_w = Calculator.textWidth(str);
    Calculator.deleteSprite();
    return px_w;
}

int Screen::CalculatefontHeight_px(const GFXfont* p_font) const {
    // Getter V1
    // TFT_eSprite Calculator = TFT_eSprite(&_tft);
    // Calculator.createSprite(_tft.px_width() * 8, _tft.height());
    // Calculator.setFreeFont(p_font);
    // return Calculator.px_height_font();

    // Getter V2
    // Check if the height has already been calculated for this p_font
    auto it = font_height_cache.find(p_font);
    if (it != font_height_cache.end()) {
        //   If yes, return the stored value
        return it->second;
    }

    const char* t =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    TFT_eSprite Calculator = TFT_eSprite(&_tft);
    Calculator.setFreeFont(p_font);

    uint16_t len = strlen(t);
    uint16_t maxH = 0;

    for (uint16_t i = 0; i < len; i++) {
        uint16_t unicode = t[i];
        uint16_t gNum = unicode - p_font->first;
        if (gNum >= p_font->last) continue;  // Skip undefined characters

        GFXglyph* glyph = &(((GFXglyph*)p_font->glyph))[gNum];
        uint16_t h = glyph->height;

        if (h > maxH) maxH = h;
    }

    int height = maxH ? maxH + 1 : 0;

    // Save the calculated value in the cache
    font_height_cache[p_font] = height;

    return height;
}

void Screen::SetMinTextSprite_px(int px_min) {
    px_min_text_sprite = min(px_min_text_sprite, px_min);
}

void Screen::FullResetScroll() {
    for (size_t i = 0; i < vec_scrolls_coords.size(); ++i) {
        for (size_t j = 0; j < vec_scrolls_coords[i].size(); ++j) {
            vec_scrolls_coords[i][j] = 0;
        }
    }

    for (size_t i = 0; i < vec_init_scrolls_coords.size(); ++i) {
        for (size_t j = 0; j < vec_init_scrolls_coords[i].size(); ++j) {
            vec_init_scrolls_coords[i][j] = false;
        }
    }
}

void Screen::SelectiveResetScroll(const std::vector<bool>& isNeedReset) {
    for (size_t i = 0; i < vec_scrolls_coords.size(); ++i) {
        for (size_t j = 0; j < vec_scrolls_coords[i].size(); ++j) {
            if (isNeedReset[i]) {
                vec_scrolls_coords[i][j] = 0;
            }
        }
    }

    for (size_t i = 0; i < vec_init_scrolls_coords.size(); ++i) {
        for (size_t j = 0; j < vec_init_scrolls_coords[i].size(); ++j) {
            if (isNeedReset[i]) {
                vec_init_scrolls_coords[i][j] = false;
            }
        }
    }
}


