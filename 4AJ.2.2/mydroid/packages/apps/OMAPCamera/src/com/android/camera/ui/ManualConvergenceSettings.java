package com.ti.omap.android.camera.ui;

import com.ti.omap.android.camera.Camera;
import com.ti.omap.android.camera.R;

import android.app.Dialog;
import android.os.Bundle;
import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;


public class ManualConvergenceSettings extends Dialog {

    private View manualConvergencePanel;
    private SeekBar manualConvergenceControl;
    private TextView manualConvergenceCaption;
    private int mManualConvergence;
    private Handler cameraHandler;
    private int mProgressBarConvergenceValue;
    private int mMinConvergence;
    private int mConvergenceValue;

    public ManualConvergenceSettings (final Context context, final Handler handler,
            int convergenceValue,
            int minConvergence, int maxConvergence,
            int stepConvergence){
        super(context);
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        // Given default values
        mManualConvergence = convergenceValue;
        mMinConvergence = minConvergence;
        mConvergenceValue = convergenceValue;

        cameraHandler = handler;
        setContentView(R.layout.manual_convergence_slider_control);

        // Views handlers
        manualConvergencePanel = findViewById(R.id.panel);
        manualConvergenceControl = (SeekBar) findViewById(R.id.convergence_seek);
        manualConvergenceControl.setMax(Math.abs(minConvergence) + maxConvergence);
        manualConvergenceControl.setKeyProgressIncrement(stepConvergence);
        manualConvergenceCaption = (TextView) findViewById(R.id.convergence_caption);

        String sCaptionConvergence = context.getString(R.string.settings_manual_convergence_caption);
        sCaptionConvergence = sCaptionConvergence + " " + Integer.toString(mManualConvergence);
        manualConvergenceCaption.setText(sCaptionConvergence);
        mProgressBarConvergenceValue = mManualConvergence + Math.abs(minConvergence);
        manualConvergenceControl.setProgress(mProgressBarConvergenceValue);

        manualConvergencePanel.setVisibility(View.VISIBLE);

        Button btn = (Button) findViewById(R.id.buttonOK);
        btn.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    if (mConvergenceValue != mManualConvergence) {
                        Message msg = new Message();
                        msg.obj = (Integer) (mManualConvergence);
                        msg.what = Camera.MANUAL_CONVERGENCE_CHANGED;
                        cameraHandler.sendMessage(msg);
                    }
                    dismiss();
                }
        });

        manualConvergenceControl.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    manualConvergenceCaption = (TextView) findViewById(R.id.convergence_caption);
                    mManualConvergence = progress + mMinConvergence;
                    String sCaption = context.getString(R.string.settings_manual_convergence_caption);
                    sCaption = sCaption + " " + Integer.toString(mManualConvergence);
                    manualConvergenceCaption.setText(sCaption);
                }
            public void onStartTrackingTouch(SeekBar seekBar) {
            }
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
    }
}
