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
import android.media.AudioManager;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;

/**
 * 
 */
public class MediaButtonIntentReceiver extends BroadcastReceiver {

    private static final int MSG_LONGPRESS_TIMEOUT = 1;
    private static final int LONG_PRESS_DELAY = 1000;

    private static long mLastClickTime = 0;
    private static boolean mDown = false;
    private static boolean mLaunched = false;
    // BLUETI_ENHANCEMENT start-
    private static final boolean BLUETI_ENHANCEMENT = getBluetiEnhancementProperty();  // BLUETI_ENHANCEMENT
    private static boolean getBluetiEnhancementProperty() {
        // Get BLUETI_ENHANCEMENT property flag
        try {
            Class c = Class.forName("android.os.SystemProperties");
            Field blueti_enhancement_field = c.getDeclaredField("BLUETI_ENHANCEMENT");
            return blueti_enhancement_field.getBoolean(c);
        } catch (IllegalAccessException e) {
            // fall through
            // If no BLUETI_ENHANCEMENT, this exeption can be ignored.
            // Else, the code will work with BLUETI_ENHANCEMENT=false and
            //    the other classes that uses the same reflection will print log for no BlueTi.
        } catch (IllegalArgumentException e) {
            // fall through
        } catch (SecurityException e) {
            // fall through
        } catch (NoSuchFieldException e) {
            // fall through
        } catch (LinkageError e) {
            Log.e("MediaButtonIntentReceiver: ", "BlueTi:  Class, android.os.SystemProperties, can't linked, " + e);
        } catch (ClassNotFoundException e) {
            Log.e("MediaButtonIntentReceiver: ", "BlueTi:  No android.os.SystemProperties Class found, " + e);
        }
        return false;
    }
    // BLUETI_ENHANCEMENT end
    private static Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_LONGPRESS_TIMEOUT:
                    if (!mLaunched) {
                        Context context = (Context)msg.obj;
                        Intent i = new Intent();
                        i.putExtra("autoshuffle", "true");
                        i.setClass(context, MusicBrowserActivity.class);
                        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
                        context.startActivity(i);
                        mLaunched = true;
                    }
                    break;
            }
        }
    };
    
    @Override
    public void onReceive(Context context, Intent intent) {
        String intentAction = intent.getAction();
        if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intentAction)) {
            Intent i = new Intent(context, MediaPlaybackService.class);
            i.setAction(MediaPlaybackService.SERVICECMD);
            i.putExtra(MediaPlaybackService.CMDNAME, MediaPlaybackService.CMDPAUSE);
            context.startService(i);
        } else if (Intent.ACTION_MEDIA_BUTTON.equals(intentAction)) {
            KeyEvent event = (KeyEvent)
                    intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            
            if (event == null) {
                return;
            }

            int keycode = event.getKeyCode();
            int action = event.getAction();
            long eventtime = event.getEventTime();

            // single quick press: pause/resume. 
            // double press: next track
            // long press: start auto-shuffle mode.
            
            String command = null;
            switch (keycode) {
                case KeyEvent.KEYCODE_MEDIA_STOP:
                    command = MediaPlaybackService.CMDSTOP;
                    break;
                case KeyEvent.KEYCODE_HEADSETHOOK:
                case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                    command = MediaPlaybackService.CMDTOGGLEPAUSE;
                    break;
                case KeyEvent.KEYCODE_MEDIA_NEXT:
                    command = MediaPlaybackService.CMDNEXT;
                    break;
                case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                    command = MediaPlaybackService.CMDPREVIOUS;
                    break;
                case KeyEvent.KEYCODE_MEDIA_PAUSE:
                    command = MediaPlaybackService.CMDPAUSE;
                    break;
                case KeyEvent.KEYCODE_MEDIA_PLAY:
                    command = MediaPlaybackService.CMDPLAY;
                    break;
            }
            if (BLUETI_ENHANCEMENT) {
                switch (keycode) {
                    case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
                        command = MediaPlaybackService.CMDFASTF;
                        break;
                    case KeyEvent.KEYCODE_MEDIA_REWIND:
                        command = MediaPlaybackService.CMDREWIND;
                        break;
                }
            }

            if (command != null) {
                if (action == KeyEvent.ACTION_DOWN) {
                    if (mDown) {
                        if ((MediaPlaybackService.CMDTOGGLEPAUSE.equals(command) ||
                                MediaPlaybackService.CMDPLAY.equals(command))
                                && mLastClickTime != 0 
                                && eventtime - mLastClickTime > LONG_PRESS_DELAY) {
                            mHandler.sendMessage(
                                    mHandler.obtainMessage(MSG_LONGPRESS_TIMEOUT, context));
                        }
                    } else if (event.getRepeatCount() == 0) {
                        // only consider the first event in a sequence, not the repeat events,
                        // so that we don't trigger in cases where the first event went to
                        // a different app (e.g. when the user ends a phone call by
                        // long pressing the headset button)

                        // The service may or may not be running, but we need to send it
                        // a command.
                        Intent i = new Intent(context, MediaPlaybackService.class);
                        i.setAction(MediaPlaybackService.SERVICECMD);
                        if (keycode == KeyEvent.KEYCODE_HEADSETHOOK &&
                                eventtime - mLastClickTime < 300) {
                            i.putExtra(MediaPlaybackService.CMDNAME, MediaPlaybackService.CMDNEXT);
                            context.startService(i);
                            mLastClickTime = 0;
                        } else {
                            i.putExtra(MediaPlaybackService.CMDNAME, command);
                            if (BLUETI_ENHANCEMENT) {
                                i.putExtra(MediaPlaybackService.STATE, MediaPlaybackService.STATEPUSHED);
                            }
                            context.startService(i);
                            mLastClickTime = eventtime;
                        }

                        mLaunched = false;
                        mDown = true;
                    }
                } else {
                    if (BLUETI_ENHANCEMENT) {
                        if((action == KeyEvent.ACTION_UP) && (command == MediaPlaybackService.CMDFASTF || command == MediaPlaybackService.CMDREWIND)) {
                            Intent i = new Intent(context, MediaPlaybackService.class);
                            i.setAction(MediaPlaybackService.SERVICECMD);
                            i.putExtra(MediaPlaybackService.CMDNAME, command);
                            i.putExtra(MediaPlaybackService.STATE, MediaPlaybackService.STATERELEASED);
                            context.startService(i);
                        }
                    }
                    mHandler.removeMessages(MSG_LONGPRESS_TIMEOUT);
                    mDown = false;
                }
                if (isOrderedBroadcast()) {
                    abortBroadcast();
                }
            }
        }
    }
}
