// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.

#include "../inc/globals.h"
#include "../inc/application.h"
#include "../inc/onboarding.h"
#include "../inc/pairing.h"
#include "../inc/telemetry.h"
#include "../inc/config.h"
#include "../inc/device.h"
#include "../inc/watchdogController.h"

int buttonState = 0;     // current state of the button
int lastButtonState = 1; // previous state of the button
int startPressed = 0;    // the time button was pressed
int endPressed = 0;      // the time button was released
int timeHold = 0;        // the time button was hold
int timeReleased = 0;    // the time button was released

bool ApplicationController::initialize()
{
    Serial.begin(9600);
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_AZURE, OUTPUT);
    pinMode(LED_USER, OUTPUT);

    char iotCentralConfig[IOT_CENTRAL_MAX_LEN] = {0};
    ConfigController::readIotCentralConfig(iotCentralConfig, IOT_CENTRAL_MAX_LEN);

    // start the state with DANGER and update to NORMAL if everything is fine.
    DeviceControl::setState(DANGER);

    assert(Globals::loopController == NULL);
    if (iotCentralConfig[0] == 0 && iotCentralConfig[2] == 0)
    {
        LOG_ERROR("No configuration found...");
        Screen.print(0, "hold A for 5 seconds to pair or press A and B for Web pairing", true);
        //Globals::loopController = OnboardingController::New();
    }
    else
    {
        WatchdogController::initialize(); // start the watchdog only for run mode
        LOG_VERBOSE("Configuration found entering telemetry mode.");
        // wait 5 secs before initiating WiFi..
        Screen.print(0, "Starting...     ");
        Screen.print(1, "  please wait   ");
        Screen.print(2, "or press A and B");
        Screen.print(3, "for 'hard' RESET");
        int wait_count = 0;
        while (wait_count++ < 2500)
        {
            loop(); // listen for A + B button press
            wait_ms(2);
        }

        Globals::loopController = TelemetryController::New(iotCentralConfig);
    }
}

bool ApplicationController::loop()
{
    // reset the device if the A and B buttons are both pressed and held
    if (DeviceControl::IsButtonClicked(USER_BUTTON_A) &&
        DeviceControl::IsButtonClicked(USER_BUTTON_B))
    {
#ifndef COMPILE_TIME_DEFINITIONS_SET
        Screen.clean();
        Screen.print(0, "RE-initializing...");
        delay(1000); //artificial pause
        Screen.clean();
        ConfigController::clearAllConfig();

        if (Globals::loopController != NULL)
        {
            delete Globals::loopController;
            Globals::loopController = NULL;
        }
        initialize();
#else
        Screen.clean();
        Screen.print(0, "Want to change?");
        Screen.print(1, "go             ");
        Screen.print(2, "azureIOTcentral");
        Screen.print(3, ".com");
        delay(3000); //artificial pause
        Screen.clean();
#endif
    }
    else if (DeviceControl::IsButtonClicked(USER_BUTTON_A) &&
             !DeviceControl::IsButtonClicked(USER_BUTTON_B)) // listen for A pressed for more than 5 seconds (quick pairing)
    {
        if (Globals::loopController != NULL && Globals::loopController->wasInitializeCompleted())
        {
            return true;
        }
        buttonState = digitalRead(USER_BUTTON_A); // read the button input
                                                  //Serial.printf("Button value %d\n",buttonState);
        if (buttonState != lastButtonState)
        { // button state changed
            startPressed = millis();
        }
        else
        {
            int curPressed = millis();
            timeHold = curPressed - startPressed;
            if (timeHold >= 5000)
            {
                Screen.clean();
                Serial.println("Entering pairing mode");
                Screen.print(0, "Pairing mode active", true);
                Globals::loopController = PairingController::New();
            }
        }
        lastButtonState = buttonState; // save state for next loop
    }
    if (Globals::loopController != NULL)
    {
        Globals::loopController->loop();
    }
}