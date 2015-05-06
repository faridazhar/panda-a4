/*
 * Android Board Identification Application
 *
 * Copyright 2011 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Written by Dan Murphy <dmurphy@ti.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package board_id.com.ti;

import board_id.com.ti.BoardIDService;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.*;
import android.provider.Settings;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

public class Board_id_Activity extends Activity {
    /** Called when the activity is first created. */
    private static final String TAG = "BoardIDActivity";
    private static final String VER_NUM = "1.0";
    public BoardIDService IDService = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        //registerForContextMenu(findViewById(R.id.gov_button));

        IDService = new BoardIDService();

        this.get_sysfs_props();
        this.set_build_props();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        int groupId = 0;
        int menuItemOrder = Menu.NONE;

        MenuItem quitmenuItem = menu.add(groupId, 0, menuItemOrder, "Quit");
        quitmenuItem.setIcon(R.drawable.exit);
        MenuItem aboutmenuItem = menu.add(groupId, 1, menuItemOrder, "About This Application");
        aboutmenuItem.setIcon(R.drawable.about);

        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
        //Quit
        case 0:
                this.onStop();
                finish();
                break;
        //Help
        case 1:
            AlertDialog.Builder helpbox = new AlertDialog.Builder(this);
            helpbox.setMessage("Texas Instruments Board Identification Application Version: " + VER_NUM);
            helpbox.setNeutralButton("Ok", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface arg0, int arg1) {
                  // Do nothing as there is nothing to do
            }
            });
            helpbox.show();
            return true;
        default:
            return super.onOptionsItemSelected(item);
        }
        return true;
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        TextView gov_val, cpu1_stat_val;
        boolean err = false;
        String test = new String();
        String cpu = new String();

        if (IDService == null)
                IDService = new BoardIDService();

        Log.d(TAG, "Got id " + item.getItemId());
        IDService.SetGovernor(item.getItemId());

        test = IDService.GetBoardProp(BoardIDService.CPU_GOV);
        gov_val = (TextView) findViewById(R.id.governor);
        gov_val.setText(R.string.curr_gov);
        gov_val.setText(gov_val.getText() + " " + test);

        cpu = IDService.GetBoardProp(BoardIDService.LINUX_CPU1_STAT);
        cpu1_stat_val = (TextView) findViewById(R.id.cpu1_status);
        cpu1_stat_val.setText(R.string.cpu1_string);
        cpu1_stat_val.setText(cpu1_stat_val.getText() + " " + cpu);

        return true;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {

        super.onCreateContextMenu(menu, v, menuInfo);
        menu.add(0, 0, 0, "Performance");
        menu.add(0, 1, 0, "Hotplug");
        menu.add(0, 2, 0, "Ondemand");
        menu.add(0, 3, 0, "Interactive");
        menu.add(0, 4, 0, "Userspace");
        menu.add(0, 5, 0, "Conservative");
        menu.add(0, 6, 0, "Powersave");
    }

    private void get_sysfs_props() {
        TextView max_freq_val, rated_freq_val, cmd_line_val, kernel_ver_val;
        TextView family_val, type_val, ver_val, gov_val, apps_val, pvr_val;
        TextView cpu1_stat_val, wl_ver_val, dpll_trim_val, rbb_trim_val;
        TextView prod_id_val, die_id_val;
        String test = new String();

        if (this.IDService == null)
                 IDService = new BoardIDService();

        test = IDService.GetBoardProp(BoardIDService.SOC_FAMILY);

        family_val = (TextView) findViewById(R.id.family);
        family_val.setText(family_val.getText() + " " +test);

        test = IDService.GetBoardProp(BoardIDService.SOC_REV);
        type_val = (TextView) findViewById(R.id.type);
        type_val.setText(type_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.SOC_TYPE);
        ver_val = (TextView) findViewById(R.id.version);
        ver_val.setText(ver_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.SOC_MAX_FREQ);
        max_freq_val = (TextView) findViewById(R.id.max_freq);
        max_freq_val.setText(max_freq_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.APPS_BOARD_REV);
        apps_val = (TextView) findViewById(R.id.apps_brd_rev);
        apps_val.setText(apps_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.CPU_MAX_FREQ);
        rated_freq_val = (TextView) findViewById(R.id.rated_freq);
        rated_freq_val.setText(rated_freq_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.CPU_GOV);
        gov_val = (TextView) findViewById(R.id.governor);
        gov_val.setText(gov_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.LINUX_VERSION);
        kernel_ver_val = (TextView) findViewById(R.id.kernel_ver);
        kernel_ver_val.setText(kernel_ver_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.LINUX_PVR_VER);
        pvr_val = (TextView) findViewById(R.id.pvr_version);
        pvr_val.setText(pvr_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.LINUX_CMDLINE);
        cmd_line_val = (TextView) findViewById(R.id.cmd_line);
        cmd_line_val.setText(cmd_line_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.LINUX_CPU1_STAT );
        cpu1_stat_val = (TextView) findViewById(R.id.cpu1_status);
        cpu1_stat_val.setText(cpu1_stat_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.WILINK_VERSION );
        wl_ver_val = (TextView) findViewById(R.id.wilink_version);
        wl_ver_val.setText(wl_ver_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.DPLL_TRIM );
        dpll_trim_val = (TextView) findViewById(R.id.dpll_trim);
        dpll_trim_val.setText(dpll_trim_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.RBB_TRIM );
        rbb_trim_val = (TextView) findViewById(R.id.rbb_trim);
        rbb_trim_val.setText(rbb_trim_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.PRODUCTION_ID );
        prod_id_val = (TextView) findViewById(R.id.prod_id);
        prod_id_val.setText(prod_id_val.getText() + " " + test);

        test = IDService.GetBoardProp(BoardIDService.DIE_ID );
        die_id_val = (TextView) findViewById(R.id.die_id);
        die_id_val.setText(die_id_val.getText() + " " + test);

    }
    private void set_build_props() {
        final TextView bootloader_ver, db_number, serial_no, board_info;
        final TextView debug_prop, build_val, crypto_val;
        String serial = Build.SERIAL;
        String boot_ver = Build.BOOTLOADER;
        String build_id = Build.DISPLAY;
        String board_id = Build.BOARD;
        String test = new String();

        if (IDService == null)
                IDService = new BoardIDService();

        test = IDService.GetProperty(1);
        build_val = (TextView) findViewById(R.id.build_type);
        build_val.setText(build_val.getText() + " " + test);

        IDService = new BoardIDService();
        test = IDService.GetProperty(5);
        crypto_val = (TextView) findViewById(R.id.crypto_state);
        crypto_val.setText(crypto_val.getText() + " " + test);

        serial_no = (TextView) findViewById(R.id.serial_no);
        if (serial != null && !serial.equals("")) {
                serial_no.setText(serial_no.getText() + " " + serial);
        } else {
                serial_no.setText(serial_no.getText() + "Not Avaiable");
        }

        bootloader_ver = (TextView) findViewById(R.id.boot_ver);
        if (boot_ver != null && !boot_ver.equals("")) {
                bootloader_ver.setText(bootloader_ver.getText() + " " + boot_ver);
        } else {
                bootloader_ver.setText(bootloader_ver.getText() + "Not Avaiable");
        }

        db_number = (TextView) findViewById(R.id.build_ver);
        if (build_id != null && !build_id.equals("")) {
                db_number.setText(db_number.getText() + " " + build_id);
        } else {
                db_number.setText(db_number.getText() + "Not Avaiable");
        }
    	
        board_info = (TextView) findViewById(R.id.board_ver);
        if (board_id != null && !board_id.equals("")) {
                board_info.setText(board_info.getText() + " " + board_id);
        } else {
                board_info.setText(board_info.getText() + "Not Avaiable");
        }
    }
}
