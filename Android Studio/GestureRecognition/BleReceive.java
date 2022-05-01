package com.example.gesturerecognition;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;


import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.widget.Toast;

import android.util.Log;

import java.util.List;
import java.util.UUID;

public class BleReceive {
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private MyScancallback scancallback;

    private BluetoothGattCallback gattCallback;
    private BluetoothGatt mGatt;
    private BluetoothGattService mService;

    private final String TEMP_SERVICE = "0003cab5-0000-1000-8000-00805f9b0131";
    private final String TEMP_DATA = "0003caa3-0000-1000-8000-00805f9b0131";
    private final String TEMP_REQ = "0003CAA3-0000-1000-8000-00805F9B0131";
    private final String NOTIFY_DESCRIPTOR = "00002902-0000-1000-8000-00805f9b34fb";


    private boolean mScanned = false;
    private final int PERMISSION_REQUEST = 100;


    private Handler handler;
    private final int SCAN_PERIOD = 10000;

    private BluetoothDevice device;

    private Context appcontext;
    private Activity appactivity;

    public interface DirectionListener {
        public void onClickDirection(int directionId);
    }

    private BleReceive.DirectionListener mListener;

    BleReceive(MainActivity activity) {
        Log.d("myApp", "BleReceive/start");
        appactivity = activity;
        appcontext = appactivity.getApplicationContext();
        mListener = activity;

        //BLE対応端末かどうかを調べる。対応していない場合はメッセージを出して終了
        if (!appcontext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Toast.makeText(appactivity, R.string.ble_not_supported, Toast.LENGTH_SHORT).show();
            activity.finish();
        }
        //Bluetoothアダプターを初期化する
        BluetoothManager manager = (BluetoothManager) appcontext.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = manager.getAdapter();

        //bluetoothの使用が許可されていない場合は許可を求める。
        if (adapter == null || !adapter.isEnabled()) {
            Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            appactivity.startActivityForResult(appactivity.getIntent(), PERMISSION_REQUEST);
        } else {
            try {
                scanner = adapter.getBluetoothLeScanner();
                scancallback = new MyScancallback();
                Log.d("myApp", "BleReceive/start30");
                //スキャニングを10秒後に停止
                handler = new Handler();
                handler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        Log.d("myApp", "BleReceive/start35");
                        scanner.stopScan(scancallback);
                        //activity.finish();
                    }
                }, SCAN_PERIOD);
                //スキャンの開始
                scanner.startScan(scancallback);
            } catch (SecurityException e) {
                Log.d("myApp", "SecurityException10");
            }
            //scanner.startScan(scancallback);
            Log.d("myApp", "BleReceive/start40");
        }
    }

    class MyScancallback extends ScanCallback {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            Log.d("myApp", "scanResult/start");
            try {
                if (mScanned == true) return;
                if (result.getDevice() == null) return;
                if (result.getDevice().getName() == null) return;
                Log.d("myApp", "onScanResult/" + result.getDevice().getName());
                if (result.getDevice().getName().contains("CapSense Button Slider")) {
                    //BLE端末情報の保持
                    device = result.getDevice();
                    mScanned = true;
                    gattCallback = new MyGattcallback();
                    device.connectGatt(appcontext, false, gattCallback);
                    //スキャン停止
                    scanner.stopScan(scancallback);
                }
            } catch (SecurityException e) {
                Log.d("myApp", "SecurityException20");
            }
        }
    }

    class MyGattcallback extends BluetoothGattCallback {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            Log.d("myApp", "onConnect: " + newState);
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d("myApp", "onConnect05");
                if (ActivityCompat.checkSelfPermission(appactivity, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    // TODO: Consider calling
                    //    ActivityCompat#requestPermissions
                    // here to request the missing permissions, and then overriding
                    //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                    //                                          int[] grantResults)
                    // to handle the case where the user grants the permission. See the documentation
                    // for ActivityCompat#requestPermissions for more details.
                    //return;
                }
                gatt.discoverServices();
                Log.d("myApp", "onConnect10");
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            Log.d("myApp", "onServicesDiscovered: " + status);
            mGatt = gatt;
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d("myApp", "onServicesDiscovered/success");
                List<BluetoothGattService> list = gatt.getServices();
                for (BluetoothGattService service : list) {
                    Log.d("myApp", "onServicesDiscovered/" + service.getUuid().toString());

                    if (service.getUuid().toString().equals(TEMP_SERVICE)) {
                        Log.d("myApp", "onServicesDiscovered/success1");
                        try {
                            mService = service;

                            Log.d("myApp", "onServicesDiscovered/success05");
                            //Descriptorの記述
                            BluetoothGattCharacteristic characteristicdata = mService.getCharacteristic(UUID.fromString(TEMP_DATA));
                            Log.d("myApp", "onServicesDiscovered/success10");
                            mGatt.setCharacteristicNotification(characteristicdata, true);
                            BluetoothGattDescriptor descriptor = characteristicdata.getDescriptor(UUID.fromString(NOTIFY_DESCRIPTOR));
                            descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                            mGatt.writeDescriptor(descriptor);
                            Log.d("myApp", "onServicesDiscovered/success70");
                        } catch (SecurityException e) {
                            Log.d("myApp", "SecurityException40");
                        }
                    } else {
                        Log.d("myApp", "onServicesDiscovered/success-NG");
                    }
                }
            }
        }

       @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic
                characteristic) {
            Log.d("myApp", "onCharacteristic/change");
            if (characteristic.getUuid().toString().equals(TEMP_DATA)) {
                Log.d("myApp", "onCharacteristic"+characteristic.getUuid().toString());
                final byte[] t = characteristic.getValue();
                Log.d("myApp", "length:" + t.length);
                Log.d("myApp", String.format("value %x %x %x", t[0], t[1], t[2]));

                // tomo stay:0 up:1 down:2 left:3 right:4
                switch (t[0]){
                    case 0:
                        mListener.onClickDirection(MainActivity.BTN_NOMOVE);
                        break;
                    case 1:
                        mListener.onClickDirection(MainActivity.BTN_UP);
                        break;
                    case 2:
                        mListener.onClickDirection(MainActivity.BTN_DOWN);
                        break;
                    case 3:
                        mListener.onClickDirection(MainActivity.BTN_LEFT);
                        break;
                    case 4:
                        mListener.onClickDirection(MainActivity.BTN_RIGHT);
                        break;

                }
                //mListener.onClickDirection(MainActivity.BTN_UP);

                appactivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                    }
                });
            }
        }
    }
}
