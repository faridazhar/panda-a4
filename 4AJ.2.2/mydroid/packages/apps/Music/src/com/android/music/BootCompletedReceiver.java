/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2011 Texas Instruments Israel Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.music;

import java.lang.reflect.Field; // BLUETI_ENHANCEMENT

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Start service at boot.
 */
public class BootCompletedReceiver extends BroadcastReceiver {

    private static final String LOGTAG = "Music-BootCompletedReceiver: ";        // BLUETI_ENHANCEMENT
    private static final boolean BLUETI_ENHANCEMENT = getBluetiEnhancementProperty();  // BLUETI_ENHANCEMENT

    @Override
    public void onReceive(Context context, Intent intent) {
        // BLUETI_ENHANCEMENT start-
        if (BLUETI_ENHANCEMENT) {
           Intent i = new Intent(context, MediaPlaybackService.class);
           Log.v(LOGTAG, "Start Music service with BLUETI_ENHANCEMENT");
           context.startService(i);
        } else {
           Log.v(LOGTAG, "No BLUETI_ENHANCEMENT");
        }
        // BLUETI_ENHANCEMENT end
    }
    // BLUETI_ENHANCEMENT start-
    private static boolean getBluetiEnhancementProperty() {
        // Get BLUETI_ENHANCEMENT property flag
        try {
            Class c = Class.forName("android.os.SystemProperties");
            Field blueti_enhancement_field = c.getDeclaredField("BLUETI_ENHANCEMENT");
            return blueti_enhancement_field.getBoolean(c);
        } catch (IllegalAccessException e) {
            // fall through
            // If no BLUETI_ENHANCEMENT, this exeption can be ignored.
            // Else, the next check will lead to the Log that prints "No BLUETI_ENHANCEMENT".
        } catch (IllegalArgumentException e) {
            // fall through
        } catch (SecurityException e) {
            // fall through
        } catch (NoSuchFieldException e) {
            // fall through
        } catch (LinkageError e) {
            Log.e(LOGTAG, "BlueTi:  Class, android.os.SystemProperties, can't linked, " + e);
        } catch (ClassNotFoundException e) {
            Log.e(LOGTAG, "BlueTi:  No android.os.SystemProperties Class found, " + e);
        }
        return false;
    }
    // BLUETI_ENHANCEMENT end
}
