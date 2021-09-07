#include "STDInclude.hpp"

namespace Game
{
    float* con_screenMin = reinterpret_cast<float*>(0xA15F48);
}

namespace Components
{
    unsigned TextRenderer::colorTableDefault[TEXT_COLOR_COUNT]
    {
        ColorRgb(0, 0, 0),          // TEXT_COLOR_BLACK
        ColorRgb(255, 92, 92),      // TEXT_COLOR_RED
        ColorRgb(0, 255, 0),        // TEXT_COLOR_GREEN
        ColorRgb(255, 255, 0),      // TEXT_COLOR_YELLOW
        ColorRgb(0, 0, 255),        // TEXT_COLOR_BLUE
        ColorRgb(0, 255, 255),      // TEXT_COLOR_LIGHT_BLUE
        ColorRgb(255, 92, 255),     // TEXT_COLOR_PINK
        ColorRgb(255, 255, 255),    // TEXT_COLOR_DEFAULT
        ColorRgb(255, 255, 255),    // TEXT_COLOR_AXIS
        ColorRgb(255, 255, 255),    // TEXT_COLOR_ALLIES
        ColorRgb(255, 255, 255),    // TEXT_COLOR_RAINBOW
        ColorRgb(255, 255, 255),    // TEXT_COLOR_SERVER
    };

    unsigned TextRenderer::colorTableNew[TEXT_COLOR_COUNT]
    {
        ColorRgb(0, 0, 0),          // TEXT_COLOR_BLACK
        ColorRgb(255, 49, 49),      // TEXT_COLOR_RED
        ColorRgb(134, 192, 0),      // TEXT_COLOR_GREEN
        ColorRgb(255, 173, 34),     // TEXT_COLOR_YELLOW
        ColorRgb(0, 135, 193),      // TEXT_COLOR_BLUE
        ColorRgb(32, 197, 255),     // TEXT_COLOR_LIGHT_BLUE
        ColorRgb(151, 80, 221),     // TEXT_COLOR_PINK
        ColorRgb(255, 255, 255),    // TEXT_COLOR_DEFAULT
        ColorRgb(255, 255, 255),    // TEXT_COLOR_AXIS
        ColorRgb(255, 255, 255),    // TEXT_COLOR_ALLIES
        ColorRgb(255, 255, 255),    // TEXT_COLOR_RAINBOW
        ColorRgb(255, 255, 255),    // TEXT_COLOR_SERVER
    };

    unsigned(*TextRenderer::currentColorTable)[TEXT_COLOR_COUNT];
    TextRenderer::FontIconAutocompleteContext TextRenderer::autocompleteContextArray[FONT_ICON_ACI_COUNT]{};

    Dvar::Var TextRenderer::cg_newColors;
    Game::dvar_t* TextRenderer::sv_customTextColor;
    Dvar::Var TextRenderer::r_colorBlind;
    Game::dvar_t* TextRenderer::g_ColorBlind_MyTeam;
    Game::dvar_t* TextRenderer::g_ColorBlind_EnemyTeam;
    Game::dvar_t** TextRenderer::con_inputBoxColor = reinterpret_cast<Game::dvar_t**>(0x9FD4BC);

    unsigned TextRenderer::HsvToRgb(HsvColor hsv)
    {
        unsigned rgb;
        unsigned char region, p, q, t;
        unsigned int h, s, v, remainder;

        if (hsv.s == 0)
        {
            rgb = ColorRgb(hsv.v, hsv.v, hsv.v);
            return rgb;
        }

        // converting to 16 bit to prevent overflow
        h = hsv.h;
        s = hsv.s;
        v = hsv.v;

        region = static_cast<uint8_t>(h / 43);
        remainder = (h - (region * 43)) * 6;

        p = static_cast<uint8_t>((v * (255 - s)) >> 8);
        q = static_cast<uint8_t>((v * (255 - ((s * remainder) >> 8))) >> 8);
        t = static_cast<uint8_t>((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);

        switch (region)
        {
        case 0:
            rgb = ColorRgb(static_cast<uint8_t>(v), t, p);
            break;
        case 1:
            rgb = ColorRgb(q, static_cast<uint8_t>(v), p);
            break;
        case 2:
            rgb = ColorRgb(p, static_cast<uint8_t>(v), t);
            break;
        case 3:
            rgb = ColorRgb(p, q, static_cast<uint8_t>(v));
            break;
        case 4:
            rgb = ColorRgb(t, p, static_cast<uint8_t>(v));
            break;
        default:
            rgb = ColorRgb(static_cast<uint8_t>(v), p, q);
            break;
        }

        return rgb;
    }

    void TextRenderer::DrawAutocompleteBox(const float x, const float y, const float w, const float h, const float* color)
    {
        const float borderColor[4]
        {
            color[0] * 0.5f,
            color[1] * 0.5f,
            color[2] * 0.5f,
            color[3]
        };

        Game::R_AddCmdDrawStretchPic(x, y, w, h, 0.0, 0.0, 0.0, 0.0, color, Game::cls->whiteMaterial);
        Game::R_AddCmdDrawStretchPic(x, y, 2.0, h, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
        Game::R_AddCmdDrawStretchPic(x + w - 2.0f, y, 2.0, h, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
        Game::R_AddCmdDrawStretchPic(x, y, w, 2.0, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
        Game::R_AddCmdDrawStretchPic(x, y + h - 2.0f, w, 2.0, 0.0, 0.0, 0.0, 0.0, borderColor, Game::cls->whiteMaterial);
    }

    void TextRenderer::UpdateAutocompleteContextResults(FontIconAutocompleteContext& context, Game::Font_s* font)
    {
        context.resultCount = 0;

        const auto* techset2d = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_TECHNIQUE_SET, "2d").techniqueSet;

        Game::DB_EnumXAssetEntries(Game::ASSET_TYPE_MATERIAL, [&context, techset2d](const Game::XAssetEntry* entry)
        {
            if (context.resultCount >= FontIconAutocompleteContext::MAX_RESULTS)
                return;

            const auto* material = entry->asset.header.material;
            if(material->techniqueSet == techset2d && std::string(material->info.name).rfind(context.lastQuery, 0) == 0)
            {
                context.results[context.resultCount++] = {
                    std::string(Utils::String::VA(":%s:", material->info.name)),
                    std::string(material->info.name)
                };
            }
        }, true, true);

        context.maxFontIconWidth = 0;
        context.maxMaterialNameWidth = 0;
        for(auto resultIndex = 0u; resultIndex < context.resultCount; resultIndex++)
        {
            const auto& result = context.results[resultIndex];
            const auto fontIconWidth = static_cast<float>(Game::R_TextWidth(result.fontIconName.c_str(), INT_MAX, font));
            const auto materialNameWidth = static_cast<float>(Game::R_TextWidth(result.materialName.c_str(), INT_MAX, font));

            if (fontIconWidth > context.maxFontIconWidth)
                context.maxFontIconWidth = fontIconWidth;
            if (materialNameWidth > context.maxMaterialNameWidth)
                context.maxMaterialNameWidth = materialNameWidth;
        }
    }

    void TextRenderer::UpdateAutocompleteContext(FontIconAutocompleteContext& context, Game::field_t* edit, Game::Font_s* font)
    {
        int fontIconStart = -1;
        for(auto i = edit->cursor - 1; i >= 0; i--)
        {
            const auto c = static_cast<unsigned char>(edit->buffer[i]);
            if (c == ':')
            {
                fontIconStart = i + 1;
                break;
            }

            if (isspace(c))
                break;

            if (c == '+')
                break;
        }

        if(fontIconStart < 0 || fontIconStart == edit->cursor)
        {
            context.autocompleteActive = false;
            context.lastHash = 0;
            context.resultCount = 0;
            return;
        }

        context.autocompleteActive = true;
        const auto currentFontIconHash = Game::R_HashString(&edit->buffer[fontIconStart], edit->cursor - fontIconStart);
        if (currentFontIconHash == context.lastHash && context.lastResultOffset == context.resultOffset)
            return;

        if(currentFontIconHash != context.lastHash)
        {
            context.resultOffset = 0;
            context.lastHash = currentFontIconHash;
        }

        context.lastQuery = std::string(&edit->buffer[fontIconStart], edit->cursor - fontIconStart);
        UpdateAutocompleteContextResults(context, font);
    }

    void TextRenderer::DrawAutocomplete(const FontIconAutocompleteContext& context, const float x, const float y, Game::Font_s* font)
    {
        const auto* text = Utils::String::VA("Font icons starting with ^2%s^7:", context.lastQuery.c_str());
        const auto boxWidth = std::max(context.maxFontIconWidth + context.maxMaterialNameWidth + FONT_ICON_AUTOCOMPLETE_COL_SPACING, 
            static_cast<float>(Game::R_TextWidth(text, INT_MAX, font)));

        const auto totalLines = 1u + context.resultCount;
        DrawAutocompleteBox(x - FONT_ICON_AUTOCOMPLETE_BOX_PADDING, 
            y - FONT_ICON_AUTOCOMPLETE_BOX_PADDING, 
            boxWidth + FONT_ICON_AUTOCOMPLETE_BOX_PADDING * 2, 
            static_cast<float>(font->pixelHeight * totalLines) + FONT_ICON_AUTOCOMPLETE_BOX_PADDING * 2,
            (*con_inputBoxColor)->current.vector);

        const float textColor[4]
        {
            1.0f,
            1.0f,
            0.8f,
            1.0f
        };

        auto currentY = y + static_cast<float>(font->pixelHeight);
        Game::R_AddCmdDrawText(text, INT_MAX, font, x, currentY, 1.0f, 1.0f, 0.0, textColor, 0);
        currentY += static_cast<float>(font->pixelHeight);

        for(auto resultIndex = 0u; resultIndex < context.resultCount; resultIndex++)
        {
            const auto& result = context.results[resultIndex];
            Game::R_AddCmdDrawText(result.fontIconName.c_str(), INT_MAX, font, x, currentY, 1.0f, 1.0f, 0.0, textColor, 0);
            Game::R_AddCmdDrawText(result.materialName.c_str(), INT_MAX, font, x + context.maxFontIconWidth + FONT_ICON_AUTOCOMPLETE_COL_SPACING, currentY, 1.0f, 1.0f, 0.0, textColor, 0);
            currentY += static_cast<float>(font->pixelHeight);
        }
    }

    void TextRenderer::Con_DrawInput_Hk(const int localClientNum)
    {
        // Call original function
        Utils::Hook::Call<void(int)>(0x5A4480)(localClientNum);

        auto& autocompleteContext = autocompleteContextArray[FONT_ICON_ACI_CONSOLE];
        UpdateAutocompleteContext(autocompleteContext, Game::g_consoleField, Game::cls->consoleFont);
        if (autocompleteContext.autocompleteActive)
        {
            const auto x = Game::conDrawInputGlob->leftX;
            const auto y = Game::con_screenMin[1] + 6.0f + static_cast<float>(2 * Game::R_TextHeight(Game::cls->consoleFont));
            DrawAutocomplete(autocompleteContext, x, y, Game::cls->consoleFont);
        }
    }

    void TextRenderer::Field_Draw_Say(const int localClientNum, Game::field_t* edit, const int x, const int y, const int horzAlign, const int vertAlign)
    {
        Game::Field_Draw(localClientNum, edit, x, y, horzAlign, vertAlign);

        auto& autocompleteContext = autocompleteContextArray[FONT_ICON_ACI_CHAT];
        UpdateAutocompleteContext(autocompleteContext, edit, Game::cls->consoleFont);
        if (autocompleteContext.autocompleteActive)
        {
            auto* screenPlacement = Game::ScrPlace_GetActivePlacement(localClientNum);
            auto xx = static_cast<float>(x);
            auto yy = static_cast<float>(y);
            auto ww = 0.0f;
            auto hh = 0.0f;
            Game::ScrPlace_ApplyRect(screenPlacement, &xx, &yy, &ww, &hh, horzAlign, vertAlign);
            yy += static_cast<float>(2 * Game::R_TextHeight(Game::cls->consoleFont));
            DrawAutocomplete(autocompleteContext, std::floor(xx), std::floor(yy), Game::cls->consoleFont);
        }
    }

    float TextRenderer::GetMonospaceWidth(Game::Font_s* font, int rendererFlags)
    {
        if(rendererFlags & Game::TEXT_RENDERFLAG_FORCEMONOSPACE)
            return Game::R_GetCharacterGlyph(font, 'o')->dx;

        return 0.0f;
    }

    void TextRenderer::GlowColor(Game::GfxColor* result, const Game::GfxColor baseColor, const Game::GfxColor forcedGlowColor, int renderFlags)
    {
        if (renderFlags & Game::TEXT_RENDERFLAG_GLOW_FORCE_COLOR)
        {
            result->array[0] = forcedGlowColor.array[0];
            result->array[1] = forcedGlowColor.array[1];
            result->array[2] = forcedGlowColor.array[2];
        }
        else
        {
            result->array[0] = static_cast<char>(std::floor(static_cast<float>(static_cast<uint8_t>(baseColor.array[0])) * 0.06f));
            result->array[1] = static_cast<char>(std::floor(static_cast<float>(static_cast<uint8_t>(baseColor.array[1])) * 0.06f));
            result->array[2] = static_cast<char>(std::floor(static_cast<float>(static_cast<uint8_t>(baseColor.array[2])) * 0.06f));
        }
    }

    unsigned TextRenderer::R_FontGetRandomLetter(const int seed)
    {
        static constexpr char RANDOM_CHARACTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
        return RANDOM_CHARACTERS[seed % (std::extent_v<decltype(RANDOM_CHARACTERS)> -1)];
    }

    void TextRenderer::DrawTextFxExtraCharacter(Game::Material* material, const int charIndex, const float x, const float y, const float w, const float h, const float sinAngle, const float cosAngle, const unsigned color)
    {
        Game::RB_DrawStretchPicRotate(material, x, y, w, h, static_cast<float>(charIndex % 16) * 0.0625f, 0.0f, static_cast<float>(charIndex % 16) * 0.0625f + 0.0625f, 1.0f, sinAngle, cosAngle, color);
    }

    Game::GfxImage* TextRenderer::GetFontIconColorMap(const Game::Material* fontIconMaterial)
    {
        for (auto i = 0u; i < fontIconMaterial->textureCount; i++)
        {
            if (fontIconMaterial->textureTable[i].nameHash == COLOR_MAP_HASH)
                return fontIconMaterial->textureTable[i].u.image;
        }

        return nullptr;
    }

    bool TextRenderer::IsFontIcon(const char*& text, FontIconInfo& fontIcon)
    {
        const auto* curPos = text;

        while (*curPos != ' ' && *curPos != ':' && *curPos != 0 && *curPos != '+')
            curPos++;

        const auto* nameEnd = curPos;
        
        if(*curPos == '+')
        {
            auto breakArgs = false;
            while(!breakArgs)
            {
                curPos++;
                switch(*curPos)
                {
                case 'h':
                    fontIcon.flipHorizontal = true;
                    break;

                case 'v':
                    fontIcon.flipVertical = true;
                    break;

                case 'b':
                    fontIcon.big = true;
                    break;

                case ':':
                    breakArgs = true;
                    break;

                default:
                    return false;
                }
            }
        }

        if (*curPos != ':')
            return false;

        const std::string fontIconName(text, nameEnd - text);

        auto* materialEntry = Game::DB_FindXAssetEntry(Game::XAssetType::ASSET_TYPE_MATERIAL, fontIconName.data());
        if (materialEntry == nullptr)
            return false;
        auto* material = materialEntry->asset.header.material;
        if (material == nullptr || material->techniqueSet == nullptr || material->techniqueSet->name == nullptr || strcmp(material->techniqueSet->name, "2d") != 0)
            return false;

        text = curPos + 1;
        fontIcon.material = material;
        return true;
    }

    float TextRenderer::GetFontIconWidth(const FontIconInfo& fontIcon, const Game::Font_s* font, const float xScale)
    {
        const auto* colorMap = GetFontIconColorMap(fontIcon.material);
        if (colorMap == nullptr)
            return 0;
        const auto sizeMultiplier = fontIcon.big ? 1.5f : 1.0f;
        return static_cast<float>(font->pixelHeight) * (static_cast<float>(colorMap->width) / static_cast<float>(colorMap->height)) * xScale * sizeMultiplier;
    }

    float TextRenderer::DrawFontIcon(const FontIconInfo& fontIcon, const float x, const float y, const float sinAngle, const float cosAngle, const Game::Font_s* font, const float xScale, const float yScale, const unsigned color)
    {
        const auto* colorMap = GetFontIconColorMap(fontIcon.material);
        if (colorMap == nullptr)
            return 0;

        float s0, t0, s1, t1;
        if(fontIcon.flipHorizontal)
        {
            s0 = 1.0f;
            s1 = 0.0f;
        }
        else
        {
            s0 = 0.0f;
            s1 = 1.0f;
        }
        if(fontIcon.flipVertical)
        {
            t0 = 1.0f;
            t1 = 0.0f;
        }
        else
        {
            t0 = 0.0f;
            t1 = 1.0f;
        }
        const auto sizeMultiplier = fontIcon.big ? 1.5f : 1.0f;

        const auto h = static_cast<float>(font->pixelHeight) * yScale * sizeMultiplier;
        const auto w = static_cast<float>(font->pixelHeight) * (static_cast<float>(colorMap->width) / static_cast<float>(colorMap->height)) * xScale * sizeMultiplier;

        const auto yy = y - (h + yScale * static_cast<float>(font->pixelHeight)) * 0.5f;
        Game::RB_DrawStretchPicRotate(fontIcon.material, x, yy, w, h, s0, t0, s1, t1, sinAngle, cosAngle, color);

        return w;
    }

    float TextRenderer::DrawHudIcon(const char*& text, const float x, const float y, const float sinAngle, const float cosAngle, const Game::Font_s* font, const float xScale, const float yScale, const unsigned color)
    {
        float s0, s1, t0, t1;

        if(*text == '\x01')
        {
            s0 = 0.0;
            t0 = 0.0;
            s1 = 1.0;
            t1 = 1.0;
        }
        else
        {
            s0 = 1.0;
            t0 = 0.0;
            s1 = 0.0;
            t1 = 1.0;
        }
        text++;

        if (*text == 0)
            return 0;

        const auto v12 = font->pixelHeight * (*text - 16) + 16;
        const auto w = static_cast<float>((((v12 >> 24) & 0x1F) + v12) >> 5) * xScale;
        text++;

        if (*text == 0)
            return 0;

        const auto h = static_cast<float>((font->pixelHeight * (*text - 16) + 16) >> 5) * yScale;
        text++;

        if (*text == 0)
            return 0;

        const auto materialNameLen = static_cast<uint8_t>(*text);
        text++;

        for(auto i = 0u; i < materialNameLen; i++)
        {
            if (text[i] == 0)
                return 0;
        }

        const std::string materialName(text, materialNameLen);
        text += materialNameLen;

        auto* material = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_MATERIAL, materialName.data()).material;
        if (material == nullptr || material->techniqueSet == nullptr || material->techniqueSet->name == nullptr || strcmp(material->techniqueSet->name, "2d") != 0)
            material = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_MATERIAL, "default").material;

        const auto yy = y - (h + yScale * static_cast<float>(font->pixelHeight)) * 0.5f;

        Game::RB_DrawStretchPicRotate(material, x, yy, w, h, s0, t0, s1, t1, sinAngle, cosAngle, color);

        return w;
    }

    void TextRenderer::RotateXY(const float cosAngle, const float sinAngle, const float pivotX, const float pivotY, const float x, const float y, float* outX, float* outY)
    {
        *outX = (x - pivotX) * cosAngle + pivotX - (y - pivotY) * sinAngle;
        *outY = (y - pivotY) * cosAngle + pivotY + (x - pivotX) * sinAngle;
    }

    void TextRenderer::DrawText2D(const char* text, float x, float y, Game::Font_s* font, float xScale, float yScale, float sinAngle, float cosAngle, Game::GfxColor color, int maxLength, int renderFlags, int cursorPos, char cursorLetter, float padding, Game::GfxColor glowForcedColor, int fxBirthTime, int fxLetterTime, int fxDecayStartTime, int fxDecayDuration, Game::Material* fxMaterial, Game::Material* fxMaterialGlow)
    {
        UpdateColorTable();

        Game::GfxColor dropShadowColor{0};
        dropShadowColor.array[3] = color.array[3];

        int randSeed = 1;
        bool drawRandomCharAtEnd = false;
        const auto forceMonospace = renderFlags & Game::TEXT_RENDERFLAG_FORCEMONOSPACE;
        const auto monospaceWidth = GetMonospaceWidth(font, renderFlags);
        auto* material = font->material;
        Game::Material* glowMaterial = nullptr;

        bool decaying;
        int decayTimeElapsed;
        if(renderFlags & Game::TEXT_RENDERFLAG_FX_DECODE)
        {
            if (!Game::SetupPulseFXVars(text, maxLength, fxBirthTime, fxLetterTime, fxDecayStartTime, fxDecayDuration, &drawRandomCharAtEnd, &randSeed, &maxLength, &decaying, &decayTimeElapsed))
                return;
        }
        else
        {
            drawRandomCharAtEnd = false;
            randSeed = 1;
            decaying = false;
            decayTimeElapsed = 0;
        }

        Game::FontPassType passes[Game::FONTPASS_COUNT];
        unsigned passCount = 0;

        if(renderFlags & Game::TEXT_RENDERFLAG_OUTLINE)
        {
            if(renderFlags & Game::TEXT_RENDERFLAG_GLOW)
            {
                glowMaterial = font->glowMaterial;
                passes[passCount++] = Game::FONTPASS_GLOW;
            }

            passes[passCount++] = Game::FONTPASS_OUTLINE;
            passes[passCount++] = Game::FONTPASS_NORMAL;
        }
        else
        {
            passes[passCount++] = Game::FONTPASS_NORMAL;

            if (renderFlags & Game::TEXT_RENDERFLAG_GLOW)
            {
                glowMaterial = font->glowMaterial;
                passes[passCount++] = Game::FONTPASS_GLOW;
            }
        }

        const auto startX = x - xScale * 0.5f;
        const auto startY = y - 0.5f * yScale;

        for(auto passIndex = 0u; passIndex < passCount; passIndex++)
        {
            float xRot, yRot;
            const char* curText = text;
            auto maxLengthRemaining = maxLength;
            auto currentColor = color;
            auto subtitleAllowGlow = false;
            auto extraFxChar = 0;
            auto drawExtraFxChar = false;
            auto passRandSeed = randSeed;
            auto count = 0;
            auto xa = startX;
            auto xy = startY;

            while(*curText && maxLengthRemaining)
            {
                if (passes[passIndex] == Game::FONTPASS_NORMAL && renderFlags & Game::TEXT_RENDERFLAG_CURSOR && count == cursorPos)
                {
                    RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);
                    Game::RB_DrawCursor(material, cursorLetter, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, color.packed);
                }

                auto letter = Game::SEH_ReadCharFromString(&curText, nullptr);

                if(letter == '^' && *curText >= COLOR_FIRST_CHAR && *curText <= COLOR_LAST_CHAR)
                {
                    const auto colorIndex = ColorIndexForChar(*curText);
                    subtitleAllowGlow = false;
                    if (colorIndex == TEXT_COLOR_DEFAULT)
                    {
                        currentColor = color;
                    }
                    else if (renderFlags & Game::TEXT_RENDERFLAG_SUBTITLETEXT && colorIndex == TEXT_COLOR_GREEN)
                    {
                        constexpr Game::GfxColor altColor{ MY_ALTCOLOR_TWO };
                        subtitleAllowGlow = true;
                        // Swap r and b for whatever reason
                        currentColor.packed = ColorRgba(altColor.array[2], altColor.array[1], altColor.array[0], Game::ModulateByteColors(altColor.array[3], color.array[3]));
                    }
                    else
                    {
                        const Game::GfxColor colorTableColor{ (*currentColorTable)[colorIndex] };
                        // Swap r and b for whatever reason
                        currentColor.packed = ColorRgba(colorTableColor.array[2], colorTableColor.array[1], colorTableColor.array[0], color.array[3]);
                    }

                    if(!(renderFlags & Game::TEXT_RENDERFLAG_CURSOR && cursorPos > count && cursorPos < count + 2))
                    {
                        curText++;
                        count += 2;
                        continue;
                    }
                }

                auto finalColor = currentColor;

                if(letter == '^' && (*curText == '\x01' || *curText == '\x02'))
                {
                    RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);
                    xa += DrawHudIcon(curText, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, finalColor.packed);

                    if (renderFlags & Game::TEXT_RENDERFLAG_PADDING)
                        xa += xScale * padding;
                    ++count;
                    maxLengthRemaining--;
                    continue;
                }

                if(letter == ':')
                {
                    FontIconInfo fontIconInfo{};
                    const char* fontIconEnd = curText;
                    if(IsFontIcon(fontIconEnd, fontIconInfo) && !(renderFlags & Game::TEXT_RENDERFLAG_CURSOR && cursorPos > count && cursorPos <= count + (fontIconEnd - curText)))
                    {
                        RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);

                        if(passes[passIndex] == Game::FONTPASS_NORMAL)
                            xa += DrawFontIcon(fontIconInfo, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, ColorRgba(255, 255, 255, finalColor.array[3]));
                        else
                            xa += GetFontIconWidth(fontIconInfo, font, xScale);

                        if (renderFlags & Game::TEXT_RENDERFLAG_PADDING)
                            xa += xScale * padding;
                        count += (fontIconEnd - curText) + 1;
                        maxLengthRemaining--;
                        curText = fontIconEnd;
                        continue;
                    }
                }

                if(drawRandomCharAtEnd && maxLengthRemaining == 1)
                {
                    letter = R_FontGetRandomLetter(Game::RandWithSeed(&passRandSeed));

                    if(Game::RandWithSeed(&passRandSeed) % 2)
                    {
                        drawExtraFxChar = true;
                        letter = 'O';
                    }
                }

                if(letter == '\n')
                {
                    xa = startX;
                    xy += static_cast<float>(font->pixelHeight) * yScale;
                    continue;
                }

                if(letter == '\r')
                {
                    xy += static_cast<float>(font->pixelHeight) * yScale;
                    continue;
                }

                auto skipDrawing = false;
                if(decaying)
                {
                    char decayAlpha;
                    Game::GetDecayingLetterInfo(letter, &passRandSeed, decayTimeElapsed, fxBirthTime, fxDecayDuration, currentColor.array[3], &skipDrawing, &decayAlpha, &letter, &drawExtraFxChar);
                    finalColor.array[3] = decayAlpha;
                }

                if(drawExtraFxChar)
                {
                    auto tempSeed = passRandSeed;
                    extraFxChar = Game::RandWithSeed(&tempSeed);
                }

                auto glyph = Game::R_GetCharacterGlyph(font, letter);
                auto xAdj = static_cast<float>(glyph->x0) * xScale;
                auto yAdj = static_cast<float>(glyph->y0) * yScale;

                if(!skipDrawing)
                {
                    if (passes[passIndex] == Game::FONTPASS_NORMAL)
                    {
                        if (renderFlags & Game::TEXT_RENDERFLAG_DROPSHADOW)
                        {
                            auto ofs = 1.0f;
                            if (renderFlags & Game::TEXT_RENDERFLAG_DROPSHADOW_EXTRA)
                                ofs += 1.0f;

                            xRot = xa + xAdj + ofs;
                            yRot = xy + yAdj + ofs;
                            RotateXY(cosAngle, sinAngle, startX, startY, xRot, yRot, &xRot, &yRot);
                            if (drawExtraFxChar)
                                DrawTextFxExtraCharacter(fxMaterial, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, dropShadowColor.packed);
                            else
                                Game::RB_DrawChar(material, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, dropShadowColor.packed);
                        }

                        RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj, xy + yAdj, &xRot, &yRot);
                        if (drawExtraFxChar)
                            DrawTextFxExtraCharacter(fxMaterial, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, finalColor.packed);
                        else
                            Game::RB_DrawChar(material, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, finalColor.packed);
                    }
                    else if(passes[passIndex] == Game::FONTPASS_OUTLINE)
                    {
                        auto outlineSize = 1.0f;
                        if (renderFlags & Game::TEXT_RENDERFLAG_OUTLINE_EXTRA)
                            outlineSize = 1.3f;

                        for (const auto offset : MY_OFFSETS)
                        {
                            RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj + outlineSize * offset[0], xy + yAdj + outlineSize * offset[1], &xRot, &yRot);
                            if (drawExtraFxChar)
                                DrawTextFxExtraCharacter(fxMaterial, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, dropShadowColor.packed);
                            else
                                Game::RB_DrawChar(material, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, dropShadowColor.packed);
                        }
                    }
                    else if(passes[passIndex] == Game::FONTPASS_GLOW && ((renderFlags & Game::TEXT_RENDERFLAG_SUBTITLETEXT) == 0 || subtitleAllowGlow))
                    {
                        GlowColor(&finalColor, finalColor, glowForcedColor, renderFlags);

                        for (const auto offset : MY_OFFSETS)
                        {
                            RotateXY(cosAngle, sinAngle, startX, startY, xa + xAdj + 2.0f * offset[0] * xScale, xy + yAdj + 2.0f * offset[1] * yScale, &xRot, &yRot);
                            if (drawExtraFxChar)
                                DrawTextFxExtraCharacter(fxMaterialGlow, extraFxChar, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, finalColor.packed);
                            else
                                Game::RB_DrawChar(glowMaterial, xRot, yRot, static_cast<float>(glyph->pixelWidth) * xScale, static_cast<float>(glyph->pixelHeight) * yScale, sinAngle, cosAngle, glyph, finalColor.packed);
                        }
                    }
                }

                if(forceMonospace)
                    xa += monospaceWidth * xScale;
                else
                    xa += static_cast<float>(glyph->dx) * xScale;

                if (renderFlags & Game::TEXT_RENDERFLAG_PADDING)
                    xa += xScale * padding;

                count++;
                maxLengthRemaining--;
            }

            if(renderFlags & Game::TEXT_RENDERFLAG_CURSOR && count == cursorPos)
            {
                RotateXY(cosAngle, sinAngle, startX, startY, xa, xy, &xRot, &yRot);
                Game::RB_DrawCursor(material, cursorLetter, xRot, yRot, sinAngle, cosAngle, font, xScale, yScale, color.packed);
            }
        }
    }

    int TextRenderer::R_TextWidth_Hk(const char* text, int maxChars, Game::Font_s* font)
    {
        auto lineWidth = 0;
        auto maxWidth = 0;

        if (maxChars <= 0)
            maxChars = 0x7FFFFFFF;

        if (text == nullptr)
            return 0;

        auto count = 0;
        while (text && *text && count < maxChars)
        {
            const auto letter = Game::SEH_ReadCharFromString(&text, nullptr);
            if (letter == '\r' || letter == '\n')
            {
                lineWidth = 0;
            }
            else
            {
                if (letter == '^' && text)
                {
                    if (*text >= COLOR_FIRST_CHAR && *text <= COLOR_LAST_CHAR)
                    {
                        text++;
                        continue;
                    }

                    if (*text >= '\x01' && *text <= '\x02' && text[1] != '\0' && text[2] != '\0' && text[3] != '\0')
                    {
                        const auto width = text[1];
                        const auto materialNameLength = text[3];

                        auto v9 = font->pixelHeight * (width - 16) + 16;
                        auto w = ((((v9 >> 24) & 0x1F) + v9) >> 5);

                        lineWidth += w;
                        if (lineWidth > maxWidth)
                            maxWidth = lineWidth;

                        text += 4;
                        for (auto currentLength = 0; currentLength < materialNameLength && *text; currentLength++)
                            text++;
                        continue;
                    }
                }

                if (letter == ':')
                {
                    FontIconInfo fontIconInfo{};
                    const char* fontIconEnd = text;
                    if (IsFontIcon(fontIconEnd, fontIconInfo))
                    {
                        lineWidth += static_cast<int>(GetFontIconWidth(fontIconInfo, font, 1.0f));
                        if (lineWidth > maxWidth)
                            maxWidth = lineWidth;
                        text = fontIconEnd;
                        continue;
                    }
                }

                lineWidth += R_GetCharacterGlyph(font, letter)->dx;
                if (lineWidth > maxWidth)
                    maxWidth = lineWidth;
                count++;
            }
        }

        return maxWidth;
    }

    unsigned int TextRenderer::ColorIndex(const char index)
    {
        auto result = index - '0';
        if (static_cast<unsigned int>(result) >= TEXT_COLOR_COUNT || result < 0) result = 7;
        return result;
    }

    void TextRenderer::StripColors(const char* in, char* out, size_t max)
    {
        if (!in || !out) return;

        max--;
        size_t current = 0;
        while (*in != 0 && current < max)
        {
            const char index = *(in + 1);
            if (*in == '^' && (ColorIndex(index) != 7 || index == '7'))
            {
                ++in;
            }
            else
            {
                *out = *in;
                ++out;
                ++current;
            }

            ++in;
        }
        *out = '\0';
    }

    std::string TextRenderer::StripColors(const std::string& in)
    {
        char buffer[1000] = { 0 }; // Should be more than enough
        StripColors(in.data(), buffer, sizeof(buffer));
        return std::string(buffer);
    }

    void TextRenderer::StripMaterialTextIcons(const char* in, char* out, size_t max)
    {
        if (!in || !out) return;

        max--;
        size_t current = 0;
        while (*in != 0 && current < max)
        {
            if (*in == '^' && (in[1] == '\x01' || in[1] == '\x02'))
            {
                in += 2;

                if (*in) // width
                    in++;
                if (*in) // height
                    in++;

                if(*in) // material name length + material name characters
                {
                    const auto materialNameLength = *in;
                    in++;
                    for(auto i = 0; i < materialNameLength; i++)
                    {
                        if (*in)
                            in++;
                    }
                }
            }
            else
            {
                *out = *in;
                ++out;
                ++current;
                ++in;
            }

        }
        *out = '\0';
    }

    std::string TextRenderer::StripMaterialTextIcons(const std::string& in)
    {
        char buffer[1000] = { 0 }; // Should be more than enough
        StripAllTextIcons(in.data(), buffer, sizeof(buffer));
        return std::string(buffer);
    }

    void TextRenderer::StripAllTextIcons(const char* in, char* out, size_t max)
    {
        if (!in || !out) return;

        max--;
        size_t current = 0;
        while (*in != 0 && current < max)
        {
            if (*in == '^' && (in[1] == '\x01' || in[1] == '\x02'))
            {
                in += 2;

                if (*in) // width
                    in++;
                if (*in) // height
                    in++;

                if(*in) // material name length + material name characters
                {
                    const auto materialNameLength = *in;
                    in++;
                    for(auto i = 0; i < materialNameLength; i++)
                    {
                        if (*in)
                            in++;
                    }
                }

                continue;
            }

            if(*in == ':')
            {
                const auto* fontIconEndPos = &in[1];
                FontIconInfo fontIcon{};
                if(IsFontIcon(fontIconEndPos, fontIcon))
                {
                    in = fontIconEndPos;
                    continue;
                }
            }

            *out = *in;
            ++out;
            ++current;
            ++in;
        }
        *out = '\0';
    }

    std::string TextRenderer::StripAllTextIcons(const std::string& in)
    {
        char buffer[1000] = { 0 }; // Should be more than enough
        StripAllTextIcons(in.data(), buffer, sizeof(buffer));
        return std::string(buffer);
    }

    int TextRenderer::SEH_PrintStrlenWithCursor(const char* string, const Game::field_t* field)
    {
        if (!string)
            return 0;

        const auto cursorPos = field->cursor;
        auto len = 0;
        auto lenWithInvisibleTail = 0;
        auto count = 0;
        const auto* curText = string;
        while(*curText)
        {
            const auto c = Game::SEH_ReadCharFromString(&curText, nullptr);
            lenWithInvisibleTail = len;

            if (c == '^' && *curText >= COLOR_FIRST_CHAR && *curText <= COLOR_LAST_CHAR && !(cursorPos > count && cursorPos < count + 2))
            {
                curText++;
                count++;
            }
            else if(c != '\r' && c != '\n')
            {
                len++;
            }

            count++;
            lenWithInvisibleTail++;
        }

        return lenWithInvisibleTail;
    }

    __declspec(naked) void TextRenderer::Field_AdjustScroll_PrintLen_Stub()
    {
        __asm
        {
            push eax
            pushad

            push esi
            push [esp + 0x8 + 0x24]
            call SEH_PrintStrlenWithCursor
            add esp, 0x8
            mov [esp + 0x20], eax

            popad
            pop eax
            ret
        }
    }

    void TextRenderer::PatchColorLimit(const char limit)
    {
        Utils::Hook::Set<char>(0x535629, limit); // DrawText2d
        Utils::Hook::Set<char>(0x4C1BE4, limit); // SEH_PrintStrlen
        Utils::Hook::Set<char>(0x4863DD, limit); // No idea :P
        Utils::Hook::Set<char>(0x486429, limit); // No idea :P
        Utils::Hook::Set<char>(0x49A5A8, limit); // No idea :P
        Utils::Hook::Set<char>(0x505721, limit); // R_TextWidth
        Utils::Hook::Set<char>(0x505801, limit); // No idea :P
        Utils::Hook::Set<char>(0x50597F, limit); // No idea :P
        Utils::Hook::Set<char>(0x5815DB, limit); // No idea :P
        Utils::Hook::Set<char>(0x592ED0, limit); // No idea :P
        Utils::Hook::Set<char>(0x5A2E2E, limit); // No idea :P

        Utils::Hook::Set<char>(0x5A2733, static_cast<char>(ColorIndexForChar(limit))); // No idea :P
    }

    // Patches team overhead normally
    bool TextRenderer::Dvar_GetUnpackedColorByName(const char* name, float* expandedColor)
    {
        if (r_colorBlind.get<bool>())
        {
            const auto str = std::string(name);
            if (str == "g_TeamColor_EnemyTeam")
            {
                // Dvar_GetUnpackedColor
                const auto* colorblindEnemy = g_ColorBlind_EnemyTeam->current.color;
                expandedColor[0] = static_cast<float>(colorblindEnemy[0]) / 255.0f;
                expandedColor[1] = static_cast<float>(colorblindEnemy[1]) / 255.0f;
                expandedColor[2] = static_cast<float>(colorblindEnemy[2]) / 255.0f;
                expandedColor[3] = static_cast<float>(colorblindEnemy[3]) / 255.0f;
                return false;
            }
            else if (str == "g_TeamColor_MyTeam")
            {
                // Dvar_GetUnpackedColor
                const auto* colorblindAlly = g_ColorBlind_MyTeam->current.color;
                expandedColor[0] = static_cast<float>(colorblindAlly[0]) / 255.0f;
                expandedColor[1] = static_cast<float>(colorblindAlly[1]) / 255.0f;
                expandedColor[2] = static_cast<float>(colorblindAlly[2]) / 255.0f;
                expandedColor[3] = static_cast<float>(colorblindAlly[3]) / 255.0f;
                return false;
            }
        }

        return true;
    }

    __declspec(naked) void TextRenderer::GetUnpackedColorByNameStub()
    {
        __asm
        {
            push[esp + 8h]
            push[esp + 8h]
            call TextRenderer::Dvar_GetUnpackedColorByName
            add esp, 8h

            test al, al
            jnz continue

            retn

        continue:
            push edi
            mov edi, [esp + 8h]
            push 406535h
            retn
        }
    }

    void TextRenderer::UpdateColorTable()
    {
        if (cg_newColors.get<bool>())
            currentColorTable = &colorTableNew;
        else
            currentColorTable = &colorTableDefault;

        (*currentColorTable)[TEXT_COLOR_AXIS] = *reinterpret_cast<unsigned*>(0x66E5F70);
        (*currentColorTable)[TEXT_COLOR_ALLIES] = *reinterpret_cast<unsigned*>(0x66E5F74);
        (*currentColorTable)[TEXT_COLOR_RAINBOW] = HsvToRgb({ static_cast<uint8_t>((Game::Sys_Milliseconds() / 200) % 256), 255,255 });
        (*currentColorTable)[TEXT_COLOR_SERVER] = sv_customTextColor->current.unsignedInt;
    }

    TextRenderer::TextRenderer()
    {
        currentColorTable = &colorTableDefault;
        
        cg_newColors = Dvar::Register<bool>("cg_newColors", true, Game::dvar_flag::DVAR_FLAG_SAVED, "Use Warfare 2 color code style.");
        sv_customTextColor = Game::Dvar_RegisterColor("sv_customTextColor", 1, 0.7f, 0, 1, Game::dvar_flag::DVAR_FLAG_REPLICATED, "Color for the extended color code.");

        // Replace vanilla text drawing function with a reimplementation with extensions
        Utils::Hook(0x535410, DrawText2D, HOOK_JUMP).install()->quick();

        // Consider material text icons and font icons when calculating text width
        Utils::Hook(0x5056C0, R_TextWidth_Hk, HOOK_JUMP).install()->quick();

        // Patch ColorIndex
        Utils::Hook(0x417770, ColorIndex, HOOK_JUMP).install()->quick();

        // Add a colorblind mode for team colors
        r_colorBlind = Dvar::Register<bool>("r_colorBlind", false, Game::dvar_flag::DVAR_FLAG_SAVED, "Use color-blindness-friendly colors");
        // A dark red
        g_ColorBlind_EnemyTeam = Game::Dvar_RegisterColor("g_ColorBlind_EnemyTeam", 0.659f, 0.088f, 0.145f, 1, Game::dvar_flag::DVAR_FLAG_SAVED, "Enemy team color for colorblind mode");
        // A bright yellow
        g_ColorBlind_MyTeam = Game::Dvar_RegisterColor("g_ColorBlind_MyTeam", 1, 0.859f, 0.125f, 1, Game::dvar_flag::DVAR_FLAG_SAVED, "Ally team color for colorblind mode");

        // Replace team colors with colorblind team colors when colorblind is enabled
        Utils::Hook(0x406530, GetUnpackedColorByNameStub, HOOK_JUMP).install()->quick();

        // Consider the cursor being inside the color escape sequence when getting the print length for a field
        Utils::Hook(0x488CBD, Field_AdjustScroll_PrintLen_Stub, HOOK_CALL).install()->quick();

        // Draw fonticon autocompletion for say field
        Utils::Hook(0x4CA1BD, Field_Draw_Say, HOOK_CALL).install()->quick();

        // Draw fonticon autocompletion for console field
        Utils::Hook(0x5A50A5, Con_DrawInput_Hk, HOOK_CALL).install()->quick();
        Utils::Hook(0x5A50BB, Con_DrawInput_Hk, HOOK_CALL).install()->quick();

        PatchColorLimit(COLOR_LAST_CHAR);
    }
}