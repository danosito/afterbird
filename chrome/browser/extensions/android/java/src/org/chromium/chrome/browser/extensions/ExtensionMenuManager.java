// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Base64;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.Map;

/**
 * Populates the main app menu with per-extension entries and resolves clicks on
 * those entries back to the extension's popup URL.
 *
 * <p>Kiwi-style feature: each running extension gets its own menu item at the
 * bottom of the menu; tapping it opens the extension's browser_action popup
 * URL in a new tab.
 */
public final class ExtensionMenuManager {
    private static final String TAG = "ExtMenu";

    /** Base item ID for extension entries. Keep well above all R.id.* values. */
    public static final int MENU_ITEM_ID_BASE = 900000;

    /** Group ID used for all extension menu items. */
    public static final int MENU_GROUP_ID = MENU_ITEM_ID_BASE;

    /** Maps extension menu item id -> popup URL (or empty if no popup). */
    private static final Map<Integer, String> sItemIdToPopupUrl = new HashMap<>();

    /** Maps extension menu item id -> extension id. */
    private static final Map<Integer, String> sItemIdToExtensionId = new HashMap<>();

    private ExtensionMenuManager() {}

    /**
     * Adds one menu entry per running extension to the given menu. Safe to
     * call multiple times; earlier mappings are cleared first.
     */
    public static void populate(Menu menu, Profile profile, WebContents webContents) {
        sItemIdToPopupUrl.clear();
        sItemIdToExtensionId.clear();

        String packed = AppMenuBridge.getRunningExtensions(profile, webContents);
        if (packed == null || packed.isEmpty()) return;

        String[] extensions = packed.split(AppMenuBridge.RECORD_SEPARATOR);
        int index = 0;
        for (String extension : extensions) {
            String[] fields = extension.split(AppMenuBridge.FIELD_SEPARATOR, -1);
            if (fields.length < 2) continue;
            String name = fields[0];
            String id = fields[1];
            String popupUrl = fields.length > 2 ? fields[2] : "";
            // fields[3] = base64 icon (unused for now)
            // fields[4] = active/inactive

            int itemId = MENU_ITEM_ID_BASE + index;
            MenuItem item = menu.add(MENU_GROUP_ID, itemId, Menu.NONE, name);
            String iconB64 = fields.length > 3 ? fields[3] : "";
            Drawable icon = decodeIcon(iconB64);
            if (icon != null) {
                item.setIcon(icon);
            }
            sItemIdToPopupUrl.put(itemId, popupUrl);
            sItemIdToExtensionId.put(itemId, id);
            index++;
        }

        if (index > 0) {
            Log.i(TAG, "[Afterbird] Populated " + index + " extension(s) in menu");
        }
    }

    /** Returns true if the menu id belongs to an extension entry. */
    public static boolean isExtensionMenuItem(int menuId) {
        return sItemIdToExtensionId.containsKey(menuId);
    }

    /** Returns the popup URL for a given extension menu item, or null. */
    public static String getPopupUrlForItem(int menuId) {
        return sItemIdToPopupUrl.get(menuId);
    }

    /** Returns the extension id for a given menu item, or null. */
    public static String getExtensionIdForItem(int menuId) {
        return sItemIdToExtensionId.get(menuId);
    }

    /**
     * Decodes a base64-encoded PNG icon into a drawable. Returns null if the
     * string is empty or the bytes don't form a valid bitmap.
     */
    private static Drawable decodeIcon(String iconB64) {
        if (iconB64 == null || iconB64.isEmpty()) return null;
        try {
            byte[] bytes = Base64.decode(iconB64, Base64.DEFAULT);
            Bitmap bitmap = BitmapFactory.decodeByteArray(bytes, 0, bytes.length);
            if (bitmap == null) return null;
            Context ctx = ContextUtils.getApplicationContext();
            return new BitmapDrawable(ctx.getResources(), bitmap);
        } catch (Throwable t) {
            Log.w(TAG, "[Afterbird] icon decode failed: " + t.getMessage());
            return null;
        }
    }
}
