/*

Hammerhead Torch

Copyright (C) 2015 Sam Hobbs

This file is part of Hammerhead Torch.

Hammerhead Torch is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Hammerhead Torch is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Hammerhead Torch.  If not, see <http://www.gnu.org/licenses/>

*/

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QDebug>
#include <QSettings>
#include <QVariant>
#include <QStandardPaths>
#include <gst/gst.h>

#include "ledcontrol.h"
#include "sailfishapp.h"

LEDControl::LEDControl(QObject *parent) :
    QObject(parent), m_isValid(false), pipeline(0), src(0), sink(0), m_gStreamerInitialised(false), m_useGStreamer(false) {
    // initialise settings object
    applicationSettings = new QSettings(); // application name defaults to binary name

    // check settings to determine whether to use gstreamer, default to true and ask the user to set it on first run
    m_useGStreamer = applicationSettings->value("useGStreamer",true).toBool();

    // initialise gstreamer pipeline
    initGstreamerTorch();

    // on hammerhead the control file is /sys/class/leds/led\:flash_torch/brightness
    // on Jolla phone it is /sys/kernel/debug/flash_adp1650/mode
    // check for path stored in app settings

    if( applicationSettings->contains("controlFilePath") && applicationSettings->contains("device") && applicationSettings->contains("brightness") )
    {
        setPath( applicationSettings->value( "controlFilePath", QString("") ).toString() );
        setDevice( applicationSettings->value( "device", QString("") ).toString() );
        setBrightness( applicationSettings->value( "brightness", QString::number(1) ).toString() );
    }
    else
    {
        // try autodetecting
        detectPath();
    }

    // get initial file state
    m_isOn = checkFile();

    // if light is not on, turn it on
    if (!m_isOn) {
        toggleState();
    }

    qDebug() << "end of constructor";
}

LEDControl::~LEDControl() {
    // turn the torch off when closing the app
    if( isOn() )
        toggleState();

    releaseGstreamerTorch();
}

QString LEDControl::getDevice()
{
    return m_device;
}

void LEDControl::setDevice(QString name)
{
    qDebug() << "Storing device name " << name;
    m_device = name;
    emit deviceChanged(m_device);

    // store the new device name in settings
    applicationSettings->setValue("device",m_device);
}

void LEDControl::detectPath()
{
    qDebug() << "detectPath called";

    // get the name of this hardware from the /etc/hw-release file
    QSettings hwFile("/etc/hw-release", QSettings::IniFormat);
    QString name = hwFile.value("NAME").toString();
    qDebug() << "Name detected from /etc/hw-release is: " << name;

    // Use the name from /etc/hw-release to look up the control file location in controlfile.conf
    QSettings controlFileSettings("/usr/share/harbour-hammerhead-torch/controlfile.conf",QSettings::IniFormat);

    // Difficult to replace /usr/share/ with a variable, because QStandardPaths doesn't support an option that resolves to /usr/share/<APPNAME> yet
    //qDebug() << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation); //QStandardPaths::AppDataLocation and QStandardPaths::AppLocalDataLocation added in Qt 5.4

    if ( controlFileSettings.contains(name) ) {
        setDevice( name );
        setPath( controlFileSettings.value(name).toString() );
    } else {
        qDebug() << "Hardware not recognised";
        setDevice("Unknown");
        setPath("Unknown");
    }

    // Use the name from /etc/hw-release to look up the brightness in brightness.conf
    QSettings brightnessSettings("/usr/share/harbour-hammerhead-torch/brightness.conf",QSettings::IniFormat);

    if ( brightnessSettings.contains(name) ) {
        setBrightness( brightnessSettings.value(name).toString() );
    } else {
        setBrightness(QString::number(1));
    }

}


QString LEDControl::getPath()
{
    qDebug() << "getPath called, current value is " << controlFilePath;
    return controlFilePath;
}


void LEDControl::setPath(QString fp)
{
    qDebug() << "Storing filepath " << fp;

    if (fp == file.fileName())
    {
        qDebug() << "filepath has not changed, doing nothing";
        return;
    }

    // check for valid file path
    QRegExp regex("/sys/.*");

    m_isValid = true;

    if ( !regex.exactMatch(fp) )
    {
        qDebug() << "validation of file path failed";
        if( !fp.startsWith("INVALID - ") && fp != "Unknown")
        {
            fp = QString("INVALID - %1").arg(fp);
        }
        m_isValid = false;
    }

    emit isValidPathChanged(m_isValid);

    // close the file
    file.close();

    // set the new filename
    controlFilePath = fp;
    emit controlFilePathChanged(controlFilePath);

    // change the file object path to the new path
    file.setFileName(fp);

    // store the new fp in settings
    applicationSettings->setValue("controlFilePath",fp);

}

bool LEDControl::checkFile()
{
    if ( !m_isValid )
        return false;

    // if the file is not readable, open it read only
    if ( file.isOpen() )
        file.close();

    file.open(QFile::ReadOnly | QIODevice::Text);

    QTextStream textStream(&file);
    QString data = textStream.readAll();

    file.close();

    if (data.toInt() == 1)
        return true;
    return false;

}

bool LEDControl::toggleState()
{
    if (m_useGStreamer) {
        if ( m_isOn ) {
            // turn off
            gst_element_set_state(pipeline, GST_STATE_NULL);
            setOnBool(!m_isOn);
        } else {
            // turn on
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
            setOnBool(!m_isOn);
        }
    } else {
        if ( !m_isValid )
            return 1;

        QString data;
        if ( m_isOn )
        {
            // turn off
            data = QString::number(0);
        }
        else
        {
            // turn on
            data = getBrightness();
        }

        if ( !file.exists() )
        {
            qDebug() << "file does not exist";
            return 1;
        }

        QFile ledFile(file.fileName());

        if ( !ledFile.open(QFile::WriteOnly) )
        {
            qDebug() << "can not open file";
            return 1;
        }

        QTextStream stream(&ledFile);
        stream << data;
        stream.flush();
        QTextStream::Status status = stream.status();
        ledFile.close();

        // check that the write succeeded before changing the state of led boolean
        if ( status != QTextStream::Ok )
        {
            qDebug() << "error writing to file";
            return 1;
        }

        // toggle the boolean using the setOnBool method, which will emit a signal and change the qml property
        setOnBool(!m_isOn);
    }

    return 0;
}

bool LEDControl::isOn()
{
    return m_isOn;
}

void LEDControl::setOnBool(bool onBool)
{
    m_isOn = onBool;
    emit isOnBoolChanged(m_isOn);
}

bool LEDControl::isValidPath()
{
    return m_isValid;
}

QString LEDControl::getBrightness()
{
    qDebug() << "getBrightness called, current value is " << m_brightness;
    return m_brightness;
}

void LEDControl::setBrightness(QString brightness)
{
    qDebug() << "Setting brightness to " << brightness;
    m_brightness = brightness;
    emit brightnessChanged(m_brightness);

    // store the new brightness in settings
    applicationSettings->setValue("brightness",m_brightness);
}

void LEDControl::initGstreamerTorch() {
    qDebug() << "GStreamer torch initialising";
    gst_init(NULL, NULL);
    src = gst_element_factory_make("droidcamsrc", "src");
    sink = gst_element_factory_make("droideglsink", "sink");
    pipeline = gst_pipeline_new ("test-pipeline");
    if (!src || !sink || !pipeline) {
        return;
    }
    // Build the pipeline
    gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
    if (gst_element_link (src, sink) != TRUE) {
        qDebug() << "Elements could not be linked!";
        gst_object_unref (pipeline);
        return;
    }
    g_object_set(G_OBJECT(src), "video-torch", 1, NULL);
    g_object_set(G_OBJECT(src), "mode", 2, NULL);
    gst_element_set_state(pipeline, GST_STATE_NULL);

    // if we get to here and haven't returned due to error, initialisation was successful
    qDebug() << "gstreamer initialised successfully";
    m_gStreamerInitialised = true;
}


void LEDControl::releaseGstreamerTorch() {
    qDebug() << "releaseGStreamerTorch called";
    if (m_gStreamerInitialised) {
        qDebug() << "releasing gstreamer objects";
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref (pipeline);
    } else {
        qDebug() << "gstreamer not initialised, no gstreamer objects to be released";
    }
}

bool LEDControl::getUseGStreamer() {
    qDebug() << "getUseGStreamer called, current value is: " << m_useGStreamer;
    return m_useGStreamer;
}

void LEDControl::setUseGStreamer(bool gstreamer) {
    qDebug() << "setUseGStreamer called, setting value to: " << gstreamer;

    if (m_useGStreamer != gstreamer) {
        // if torch is on, turn it off before changing the method
        if (isOn() )
            toggleState();
        applicationSettings->setValue("useGStreamer",gstreamer);// store in settings
        m_useGStreamer = gstreamer;
    }
}

bool LEDControl::getGSTDialogCompleted() {

    bool completed = applicationSettings->value("GSTDialogCompleted",false).toBool();
    qDebug() << "getGSTDialogCompleted called, value is" << completed;
    return completed;
}

void LEDControl::setGSTDialogCompleted() {
    qDebug() << "setGSTDialogCompleted called";
    applicationSettings->setValue("GSTDialogCompleted",true);
}
