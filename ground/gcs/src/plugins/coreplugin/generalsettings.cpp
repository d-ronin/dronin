/**
 ******************************************************************************
 *
 * @file       generalsettings.cpp
 * @author     dRonin, http://dronin.org Copyright (C) 2015
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 *             Parts by Nokia Corporation (qt-info@nokia.com) Copyright (C) 2009.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup CorePlugin Core Plugin
 * @{
 * @brief The Core GCS plugin
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "generalsettings.h"

#include <utils/stylehelper.h>
#include <utils/qtcolorbutton.h>
#include <utils/consoleprocess.h>
#include <utils/pathutils.h>
#include <coreplugin/icore.h>
#include <QMessageBox>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QSettings>
#include <QDialog>
#include <QFile>
#include "ui_generalsettings.h"

using namespace Utils;
using namespace Core::Internal;

GeneralSettings::GeneralSettings():
    m_saveSettingsOnExit(true),
    m_autoConnect(true),
    m_autoSelect(true),
    m_useUDPMirror(false),
    m_useExpertMode(false),
    m_dialog(0),
    m_proxyType(QNetworkProxy::NoProxy),
    m_proxyPort(0),
    m_useSessionManaging(true),
    m_usePortableSettings(false),
    m_dontSaveOnce(false)
{
}

QString GeneralSettings::id() const
{
    return QLatin1String("General");
}

QString GeneralSettings::trName() const
{
    return tr("General");
}

QString GeneralSettings::category() const
{
    return QLatin1String("Environment");
}

QString GeneralSettings::trCategory() const
{
    return tr("Environment");
}

static bool hasQmFilesForLocale(const QString &locale, const QString &creatorTrPath)
{
    static const QString qtTrPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);

    const QString trFile = QLatin1String("qt_") + locale + QLatin1String(".qm");
    return QFile::exists(qtTrPath+'/'+trFile) || QFile::exists(creatorTrPath+'/'+trFile);
}

void GeneralSettings::fillLanguageBox() const
{
    const QString currentLocale = language();

    m_page->languageBox->addItem(tr("<System Language>"), QString());
    // need to add this explicitly, since there is no qm file for English
    m_page->languageBox->addItem(QLatin1String("English"), QLatin1String("C"));
    if (currentLocale == QLatin1String("C"))
        m_page->languageBox->setCurrentIndex(m_page->languageBox->count() - 1);

    const QString creatorTrPath =
            Core::ICore::instance()->resourcePath() + QLatin1String("/translations");
    const QStringList languageFiles = QDir(creatorTrPath).entryList(QStringList(QLatin1String("dronin*.qm")));

    Q_FOREACH(const QString &languageFile, languageFiles)
    {
        int start = languageFile.indexOf(QLatin1Char('_'))+1;
        int end = languageFile.lastIndexOf(QLatin1Char('.'));
        const QString locale = languageFile.mid(start, end-start);
        // no need to show a language that creator will not load anyway
        if (hasQmFilesForLocale(locale, creatorTrPath)) {
            m_page->languageBox->addItem(QLocale::languageToString(QLocale(locale).language()), locale);
            if (locale == currentLocale)
                m_page->languageBox->setCurrentIndex(m_page->languageBox->count() - 1);
        }
    }
}

void GeneralSettings::fillProxyTypesBox() const
{
    m_page->proxyTypeCB->addItem("No Proxy", (int)QNetworkProxy::NoProxy);
    m_page->proxyTypeCB->addItem("Socks5Proxy", (int)QNetworkProxy::Socks5Proxy);
    m_page->proxyTypeCB->addItem("HttpProxy", (int)QNetworkProxy::HttpProxy);
    m_page->proxyTypeCB->addItem("HttpCachingProxy", (int)QNetworkProxy::HttpCachingProxy);
    m_page->proxyTypeCB->addItem("FtpCachingProxy", (int)QNetworkProxy::FtpCachingProxy);
}

QWidget *GeneralSettings::createPage(QWidget *parent)
{
    m_page = new Ui::GeneralSettings();
    QWidget *w = new QWidget(parent);
    m_page->setupUi(w);
    fillLanguageBox();
    fillProxyTypesBox();
    connect(m_page->checkAutoConnect,SIGNAL(stateChanged(int)),this,SLOT(slotAutoConnect(int)));
    m_page->checkBoxSaveOnExit->setChecked(m_saveSettingsOnExit);
    m_page->checkAutoConnect->setChecked(m_autoConnect);
    m_page->checkAutoSelect->setChecked(m_autoSelect);
    m_page->cbUseUDPMirror->setChecked(m_useUDPMirror);
    m_page->cbExpertMode->setChecked(m_useExpertMode);
    m_page->cbSessionMessaging->setChecked(m_useSessionManaging);
    m_page->colorButton->setColor(StyleHelper::baseColor());
    m_page->proxyTypeCB->setCurrentIndex(m_page->proxyTypeCB->findData(m_proxyType));
    m_page->portLE->setText(QString::number(m_proxyPort));
    m_page->hostNameLE->setText(m_proxyHostname);
    m_page->userLE->setText(m_proxyUser);
    m_page->passwordLE->setText(m_proxyPassword);
    bool isWritable;
    m_usePortableSettings = (Utils::PathUtils::getInstance()->getSettingsFilename() == Utils::PathUtils::getInstance()->getLocalSettingsFilePath(isWritable));
    if(!m_usePortableSettings && !isWritable) {
        m_page->cb_usePortableSettings->setVisible(false);
        m_page->lbl_PortableSettings->setVisible(false);
    }
    m_page->cb_usePortableSettings->setChecked(m_usePortableSettings);
    connect(m_page->resetButton, SIGNAL(clicked()),
            this, SLOT(resetInterfaceColor()));
    return w;
}

void GeneralSettings::apply()
{
    int currentIndex = m_page->languageBox->currentIndex();
    setLanguage(m_page->languageBox->itemData(currentIndex, Qt::UserRole).toString());
    // Apply the new base color if accepted
    StyleHelper::setBaseColor(m_page->colorButton->color());

    m_saveSettingsOnExit = m_page->checkBoxSaveOnExit->isChecked();
    m_useUDPMirror = m_page->cbUseUDPMirror->isChecked();
    m_useExpertMode = m_page->cbExpertMode->isChecked();
    m_useSessionManaging = m_page->cbSessionMessaging->isChecked();
    m_autoConnect = m_page->checkAutoConnect->isChecked();
    m_autoSelect = m_page->checkAutoSelect->isChecked();
    m_proxyType = m_page->proxyTypeCB->currentData().toInt();
    m_proxyPort = m_page->portLE->text().toInt();
    m_proxyHostname = m_page->hostNameLE->text();
    m_proxyUser = m_page->userLE->text();
    m_proxyPassword = m_page->passwordLE->text();
    QNetworkProxy::setApplicationProxy (getNetworkProxy());
    if(!m_usePortableSettings && m_page->cb_usePortableSettings->isChecked()) {
        bool writable;
        Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable);
        if(!writable) {
            QMessageBox::warning(NULL, tr(SETTINGS_DIR_NOT_WRITABLE_MSG1), tr(SETTINGS_DIR_NOT_WRITABLE_MSG2));
            m_usePortableSettings = false;
        }
        else {
            if(QFileInfo(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable) + "_bkp").exists()) {
                QFile::remove(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable));
                QFile::rename(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable) + "_bkp", Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable));
            }
            else if(!QFileInfo(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable)).exists()) {
                QDir().mkpath(QFileInfo(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable)).absolutePath());
                QFile::copy(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath(), Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable));
            }
            QFile::rename(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath(), Utils::PathUtils::getInstance()->getGlobalSettingsFilePath() + "_bkp");
            m_dontSaveOnce = true;
        }
    }
    else if(m_usePortableSettings && !m_page->cb_usePortableSettings->isChecked()) {
        bool writable;
        if(QFileInfo(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath() + "_bkp").exists()) {
            QFile::remove(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath());
            QFile::rename(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath() + "_bkp", Utils::PathUtils::getInstance()->getGlobalSettingsFilePath());
        }
        if(!QFileInfo(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath()).exists()) {
            QDir().mkpath(QFileInfo(Utils::PathUtils::getInstance()->getGlobalSettingsFilePath()).absolutePath());
            QFile::copy(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable), Utils::PathUtils::getInstance()->getGlobalSettingsFilePath());
        }
        QFile::rename(Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable), Utils::PathUtils::getInstance()->getLocalSettingsFilePath(writable) + "_bkp");
        m_dontSaveOnce = true;
    }
    m_usePortableSettings = m_page->cb_usePortableSettings->isChecked();
    emit generalSettingsChanged();
}

void GeneralSettings::finish()
{
    delete m_page;
}

void GeneralSettings::readSettings(QSettings* qs)
{
    qs->beginGroup(QLatin1String("General"));
    m_language = qs->value(QLatin1String("OverrideLanguage"),QLocale::system().name()).toString();
    m_saveSettingsOnExit = qs->value(QLatin1String("SaveSettingsOnExit"),m_saveSettingsOnExit).toBool();
    m_autoConnect = qs->value(QLatin1String("AutoConnect"),m_autoConnect).toBool();
    m_autoSelect = qs->value(QLatin1String("AutoSelect"),m_autoSelect).toBool();
    m_useUDPMirror = qs->value(QLatin1String("UDPMirror"),m_useUDPMirror).toBool();
    m_useExpertMode = qs->value(QLatin1String("ExpertMode"),m_useExpertMode).toBool();
    m_useSessionManaging = qs->value(QLatin1String("UseSessionManaging"), m_useSessionManaging).toBool();
    m_proxyType = qs->value(QLatin1String("proxytype"),m_proxyType).toInt();
    m_proxyPort = qs->value(QLatin1String("proxyport"),m_proxyPort).toInt();
    m_proxyHostname = qs->value(QLatin1String("proxyhostname"),m_proxyHostname).toString();
    m_proxyUser = qs->value(QLatin1String("proxyuser"),m_proxyUser).toString();
    m_proxyPassword = qs->value(QLatin1String("proxypassword"),m_proxyPassword).toString();
    m_observations = qs->value(QLatin1String("observations"), "").toString();
    m_vehicle = qs->value(QLatin1String("vehicle"), "").toString();
    m_board = qs->value(QLatin1String("board"), "").toString();
    m_weight = qs->value(QLatin1String("weight"), 0).toInt();
    m_size = qs->value(QLatin1String("size"), 0).toInt();
    m_cells = qs->value(QLatin1String("cells"), 0).toInt();
    m_motors = qs->value(QLatin1String("motors"), "").toString();
    m_escs = qs->value(QLatin1String("escs"), "").toString();
    m_props = qs->value(QLatin1String("props"), "").toString();
    qs->endGroup();
    emit generalSettingsChanged();
}

void GeneralSettings::saveSettings(QSettings* qs)
{
    qs->beginGroup(QLatin1String("General"));

    if (m_language.isEmpty())
        qs->remove(QLatin1String("OverrideLanguage"));
    else
        qs->setValue(QLatin1String("OverrideLanguage"), m_language);

    qs->setValue(QLatin1String("SaveSettingsOnExit"), m_saveSettingsOnExit);
    qs->setValue(QLatin1String("AutoConnect"), m_autoConnect);
    qs->setValue(QLatin1String("AutoSelect"), m_autoSelect);
    qs->setValue(QLatin1String("UDPMirror"), m_useUDPMirror);
    qs->setValue(QLatin1String("ExpertMode"), m_useExpertMode);
    qs->setValue(QLatin1String("UseSessionManaging"), m_useSessionManaging);

    qs->setValue(QLatin1String("proxytype"), m_proxyType);
    qs->setValue(QLatin1String("proxyport"), m_proxyPort);
    qs->setValue(QLatin1String("proxyhostname"), m_proxyHostname);
    qs->setValue(QLatin1String("proxyuser"), m_proxyUser);
    qs->setValue(QLatin1String("proxypassword"), m_proxyPassword);
    qs->setValue(QLatin1String("observations"), m_observations);
    qs->setValue(QLatin1String("vehicle"), m_vehicle);
    qs->setValue(QLatin1String("board"), m_board);
    qs->setValue(QLatin1String("weight"), m_weight);
    qs->setValue(QLatin1String("size"), m_size);
    qs->setValue(QLatin1String("cells"), m_cells);
    qs->setValue(QLatin1String("motors"), m_motors);
    qs->setValue(QLatin1String("escs"), m_escs);
    qs->setValue(QLatin1String("props"), m_props);
    qs->endGroup();
}

void GeneralSettings::resetInterfaceColor()
{
    m_page->colorButton->setColor(0x666666);
}

void GeneralSettings::showHelpForExternalEditor()
{
    if (m_dialog) {
        m_dialog->show();
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }
}

void GeneralSettings::resetLanguage()
{
    // system language is default
    m_page->languageBox->setCurrentIndex(0);
}

QString GeneralSettings::language() const
{
    return m_language;
}

void GeneralSettings::setLanguage(const QString &locale)
{
    if (m_language != locale) {
        if (!locale.isEmpty()) {
        QMessageBox::information((QWidget*)Core::ICore::instance()->mainWindow(), tr("Restart required"),
                                 tr("The language change will take effect after a restart of the GCS."));
        }
        m_language = locale;
    }
}
bool GeneralSettings::getUsePortableSettings() const
{
    return m_usePortableSettings;
}

void GeneralSettings::setUsePortableSettings(bool usePortableSettings)
{
    m_usePortableSettings = usePortableSettings;
}


bool GeneralSettings::saveSettingsOnExit() const
{
    return m_saveSettingsOnExit && !m_dontSaveOnce;
}

bool GeneralSettings::autoConnect() const
{
    return m_autoConnect;
}

bool GeneralSettings::autoSelect() const
{
    return m_autoSelect;
}

bool GeneralSettings::useUDPMirror() const
{
    return m_useUDPMirror;
}

bool GeneralSettings::useSessionManaging() const
{
    return m_useSessionManaging;
}

bool GeneralSettings::useExpertMode() const
{
    return m_useExpertMode;
}

QNetworkProxy GeneralSettings::getNetworkProxy()
{
    return QNetworkProxy((QNetworkProxy::ProxyType)m_proxyType, m_proxyHostname, m_proxyPort, m_proxyUser, m_proxyPassword);
}

void GeneralSettings::setObservations(QString value)
{
    m_observations = value;
}

void GeneralSettings::setVehicleType(QString value)
{
    m_vehicle = value;
}

QString GeneralSettings::getObservations()
{
    return m_observations;
}

QString GeneralSettings::getVehicleType()
{
    return m_vehicle;
}

void GeneralSettings::setBoardType(QString type)
{
    m_board = type;
}

QString GeneralSettings::getBoardType()
{
    return m_board;
}

void GeneralSettings::setWeight(int weight)
{
    m_weight = weight;
}

int GeneralSettings::getWeight() {
    return m_weight;
}

void GeneralSettings::setVehicleSize(int size) {
    m_size = size;
}

int GeneralSettings::getVehicleSize() {
    return m_size;
}

void GeneralSettings::setBatteryCells(int cells) {
    m_cells = cells;
}

int GeneralSettings::getBatteryCells() {
    return m_cells;
}

void GeneralSettings::setMotors(QString motors) {
    m_motors = motors;
}

QString GeneralSettings::getMotors() {
    return m_motors;
}

void GeneralSettings::setESCs(QString escs) {
    m_escs = escs;
}

QString GeneralSettings::getESCs() {
    return m_escs;
}

void GeneralSettings::setProps(QString props) {
    m_props = props;
}

QString GeneralSettings::getProps() {
    return m_props;
}

void GeneralSettings::slotAutoConnect(int value)
{
    if (value==Qt::Checked)
        m_page->checkAutoSelect->setEnabled(false);
    else
        m_page->checkAutoSelect->setEnabled(true);
}
