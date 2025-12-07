/**
 * ShitBird Firmware - LVGL Configuration
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Color depth
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

// Memory
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (64 * 1024U)
#define LV_MEM_ADR              0
#define LV_MEM_AUTO_DEFRAG      1

// Display
#define LV_HOR_RES_MAX          320
#define LV_VER_RES_MAX          240
#define LV_DPI_DEF              130

// Features
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0

// Logging
#define LV_USE_LOG              0
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN

// Fonts
#define LV_FONT_MONTSERRAT_8    1
#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0

#define LV_FONT_UNSCII_8        1
#define LV_FONT_UNSCII_16       1

#define LV_FONT_DEFAULT         &lv_font_montserrat_14

// Text
#define LV_TXT_ENC              LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS      " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD        "#"

// Widgets
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        1
#define LV_USE_CANVAS           1
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMG              1
#define LV_USE_LABEL            1
#define LV_USE_LINE             1
#define LV_USE_ROLLER           1
#define LV_USE_SLIDER           1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         1
#define LV_USE_TABLE            1

// Extra widgets
#define LV_USE_ANIMIMG          1
#define LV_USE_CALENDAR         0
#define LV_USE_CHART            1
#define LV_USE_COLORWHEEL       1
#define LV_USE_IMGBTN           1
#define LV_USE_KEYBOARD         1
#define LV_USE_LED              1
#define LV_USE_LIST             1
#define LV_USE_MENU             1
#define LV_USE_METER            1
#define LV_USE_MSGBOX           1
#define LV_USE_SPAN             1
#define LV_USE_SPINBOX          1
#define LV_USE_SPINNER          1
#define LV_USE_TABVIEW          1
#define LV_USE_TILEVIEW         1
#define LV_USE_WIN              1

// Themes
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1
#define LV_THEME_DEFAULT_GROW   1

// Layouts
#define LV_USE_FLEX             1
#define LV_USE_GRID             1

// Other
#define LV_USE_SNAPSHOT         1

#endif // LV_CONF_H
