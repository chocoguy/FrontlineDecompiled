// IStudio text control.
#include "game/TibText.h"
#include "game/ShellMenu.h"
#include "game/Screen.h"
#include "game/UISUtils.h"
#include "game/color.h"
#include "game/font.h"
#include "game/isexportdefs.h"
#include "game/matstack.h"
#include "game/toilet.h"
#include "game/uistypes.h"
#include "ea/sys/fontdraw.h"
#include "string.h"

// Whether `text` has a line break before its last character.
static inline bool IsMultiLine(const char* text) {
    int len = strlen(text);
    char* cr = strrchr(text, '\r');
    int end = cr != NULL ? (cr - text) + 1 : len;
    return len != end;
}

// Draws CR LF separated lines downward from (x, y).
static inline void DrawLines(CFont* font, float x, float y, const char* text, float lineHeight) {
    int left = strlen(text);
    while (left > 0) {
        char line[0x80];
        memset(line, 0, sizeof(line));
        char* cr = strchr(text, '\r');
        if (cr != NULL) {
            int n = cr - text;
            left = left - n - 2;
            memcpy(line, text, n);
            font->DrawText(x, y, line);
            y += lineHeight;
            text = cr + 2;
        } else {
            int n = strlen(text);
            left -= n;
            memcpy(line, text, n);
            font->DrawText(x, y, line);
            y += lineHeight;
        }
    }
}

void TibTextProcessFnc(void* p, unsigned long event, unsigned long numParams, UISParam_t* params,
                       UISParam_t* results) {
    TibTextGraphicDataT* data = (TibTextGraphicDataT*)p;

    switch (event) {
    case (unsigned long)-1: {
        IStudioLibrary* lib = &LibraryInfo[data->library];
        if (lib->images[data->fontIndex] == NULL) {
            const char* file =
                (const char*)TLT_LoadFileFromShellBigFile(lib->imageNames[data->fontIndex], NULL);
            CFont* font = new CFont(file, (ERenderPriority)0x6F);
            font->SetColor(CColor(data->color[0], data->color[1], data->color[2], 0x80));
            font->SetDepth(g_screen.GetDepthBufferMax());
            font->SetHeight(data->height);
            LibraryInfo[data->library].images[data->fontIndex] = (CSprite*)font;
        }
        data->string = (UISString_t*)((u8*)data + (u32)data->string);
        break;
    }
    case (unsigned long)-2:
        TibTextDraw(data);
        break;
    case (unsigned long)-3: {
        CFont* font = (CFont*)LibraryInfo[data->library].images[data->fontIndex];
        if (font != NULL) {
            delete font;
            LibraryInfo[data->library].images[data->fontIndex] = NULL;
        }
        data->string = (UISString_t*)((u8*)data->string - (u8*)data);
        break;
    }
    case 0:
        data->color[3] = (s32)params[3].value;
        data->color[2] = (s32)params[2].value;
        data->color[1] = (s32)params[1].value;
        data->color[0] = (s32)params[0].value;
        break;
    case 1:
        data->color2[3] = (s32)params[3].value;
        data->color2[2] = (s32)params[2].value;
        data->color2[1] = (s32)params[1].value;
        data->color2[0] = (s32)params[0].value;
        break;
    case 2:
        data->field_0x40[0] = *(f32*)&params[2].value;
        data->field_0x40[1] = *(f32*)&params[1].value;
        data->field_0x40[2] = *(f32*)&params[0].value;
        break;
    case 3: {
        UISString_t* src = (UISString_t*)params[0].value;
        strncpy(data->string->text, src->text, data->string->maxLength);
        if (src->maxLength < data->string->maxLength) {
            data->string->text[src->maxLength] = '\0';
        }
        break;
    }
    case 4:
        break;
    case 5:
        data->x = *(f32*)&params[0].value;
        break;
    case 6:
        data->x = *(f32*)&params[0].value;
        data->y = *(f32*)&params[1].value;
        break;
    }
}

void TibTextDraw(TibTextGraphicDataT* data) {
    if (g_Shell.field_0x19A7 && !g_Shell.field_0x199B) {
        return;
    }
    float* add = UISGetColorAdditive();
    float* mul = UISGetColorMultipler();
    if (data->library < 0) {
        return;
    }

    CFont* font = (CFont*)LibraryInfo[data->library].images[data->fontIndex];
    font->SetColor(CColor(mul[0] * (data->color[0] + add[0]), mul[1] * (data->color[1] + add[1]),
                          mul[2] * (data->color[2] + add[2]),
                          0.5f * (mul[3] * (data->color[3] + add[3]))));
    font->SetScale(1.0f, 1.0f);

    CVector3 screen(0.0f, 0.0f, 0.0f);
    CVector3 pos(0.0f, 0.0f, 0.0f);
    pos.x = data->x;
    pos.y = data->y;
    g_matStack->Top()->TransformPoint(screen, pos);

    const char* text;
    float rx, ry, w, h;
    float x, y;
    if (data->align == TIBTEXT_ALIGN_CENTER) {
        FONT_getrecta(font->m_font, data->string->text, &rx, &ry, &w, &h);
        // The original offsets x (not y) by half the height too.
        screen.x = screen.x - 0.5f * w;
        screen.x = screen.x - 0.5f * h;
        screen.y = screen.y * g_screen.m_heightScale;
        screen.x = screen.x * g_screen.m_widthScale;
        text = data->string->text;
        if (IsMultiLine(text)) {
            DrawLines(font, screen.x, screen.y, data->string->text, h);
            return;
        }
        font->DrawText(screen.x, screen.y, data->string->text);
    } else if (data->align == TIBTEXT_ALIGN_RIGHT) {
        text = data->string->text;
        bool multi = IsMultiLine(text);
        FONT_getrecta(font->m_font, data->string->text, &rx, &ry, &w, &h);
        screen.x = screen.x - w;
        screen.x = screen.x - h;
        y = screen.y * g_screen.m_heightScale;
        x = screen.x * g_screen.m_widthScale;
        screen.y = y;
        screen.x = x;
        if (multi) {
            DrawLines(font, x, y, data->string->text, h);
            return;
        }
        font->DrawText(x, y, data->string->text);
    } else {
        text = data->string->text;
        bool multi = IsMultiLine(text);
        FONT_getrecta(font->m_font, data->string->text, &rx, &ry, &w, &h);
        x = screen.x * g_screen.m_widthScale;
        y = screen.y * g_screen.m_heightScale;
        screen.x = x;
        screen.y = y;
        if (multi) {
            DrawLines(font, x, y, data->string->text, h);
            return;
        }
        font->DrawText(x, y, data->string->text);
    }
}
