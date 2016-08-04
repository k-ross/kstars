/** *************************************************************************
                          kstarslite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 30/04/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarslite.h"
#include "skymaplite.h"
#include "kstarsdata.h"
#include <QQmlContext>
#include <QApplication>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include "indi/clientmanagerlite.h"
#include "kstarslite/imageprovider.h"
#include "klocalizedcontext.h"

#include "kspaths.h"

//Dialog
#include "kstarslite/dialogs/finddialoglite.h"

#include "Options.h"
#include "ksutils.h"

KStarsLite *KStarsLite::pinstance = 0;

KStarsLite::KStarsLite( bool doSplash, bool startClock, const QString &startDateString) {
    // Initialize logging settings
    /*if (Options::disableLogging())
        KSUtils::Logging::Disable();
    else if (Options::logToFile() && Options::verboseLogFile().isEmpty() == false)
        KSUtils::Logging::UseFile(Options::verboseLogFile());
    else
        KSUtils::Logging::UseDefault();*/

    // Set pinstance to yourself
    // Unlike KStars class we set pinstance at the beginning because SkyMapLite needs access to ClientManagerLite
    pinstance = this;

    m_KStarsData = KStarsData::Create();
    Q_ASSERT( m_KStarsData );

#ifdef INDI_FOUND
    //INDI Android Client
    m_clientManager = new ClientManagerLite;
    m_Engine.rootContext()->setContextProperty("ClientManagerLite", m_clientManager);
#endif

    //Make instance of KStarsLite and KStarsData available to QML
    m_Engine.rootContext()->setContextProperty("KStarsLite", this);
    m_Engine.rootContext()->setContextProperty("KStarsData", m_KStarsData);
    m_Engine.rootContext()->setContextProperty("Options", Options::self());
    m_Engine.rootContext()->setContextObject(new KLocalizedContext(this));

    //Dialogs
    m_findDialogLite = new FindDialogLite;
    m_Engine.rootContext()->setContextProperty("FindDialogLite", m_findDialogLite);

    //Set Geographic Location from Options
    m_KStarsData->setLocationFromOptions();

    /*SkyMapLite has to be loaded before KStarsData is initialized because SkyComponents derived classes
    have to add SkyItems to the SkyMapLite*/
    m_SkyMapLite = SkyMapLite::createInstance();

    m_Engine.rootContext()->setContextProperty("SkyMapLite", m_SkyMapLite);
    m_imgProvider = new ImageProvider;
    m_Engine.addImageProvider(QLatin1String("images"), m_imgProvider);
    //qmlRegisterType<SkyPoint>("skymaplite",1,0,"SkyMapLite");    

#ifdef Q_OS_ANDROID
    QString main = KSPaths::locate(QStandardPaths::AppDataLocation, "kstarslite/qml/main.qml");
#else
    QString main = QString(QML_IMPORT) + QString("/kstarslite/qml/main.qml");
#endif

    m_Engine.load(QUrl(main));
    Q_ASSERT_X(m_Engine.rootObjects().size(),"loading root object of main.qml",
               "QML file was not loaded. Probably syntax error or failed module import.");

    m_RootObject = m_Engine.rootObjects()[0];

    QQuickItem *skyMapLiteWrapper = m_RootObject->findChild<QQuickItem*>("skyMapLiteWrapper");
    m_SkyMapLite->setParentItem(skyMapLiteWrapper);

    // Whenever the wrapper's(parent) dimensions changed, change SkyMapLite too
    connect(skyMapLiteWrapper, &QQuickItem::widthChanged, m_SkyMapLite, &SkyMapLite::resizeItem);
    connect(skyMapLiteWrapper, &QQuickItem::heightChanged, m_SkyMapLite, &SkyMapLite::resizeItem);

    m_SkyMapLite->resizeItem(); /* Set initial size pf SkyMapLite. Without it on Android SkyMapLite is
    not displayed until screen orientation is not changed */

    //QQuickWindow *mainWindow = m_RootObject->findChild<QQuickWindow*>("mainWindow");
    QQuickWindow *mainWindow = static_cast<QQuickWindow *>(m_Engine.rootObjects()[0]);

    QSurfaceFormat format = mainWindow->format();
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::TripleBuffer);
    mainWindow->setFormat(format);

    connect( qApp, SIGNAL( aboutToQuit() ), this, SLOT( slotAboutToQuit() ) );

    //Initialize Time and Date
    if (startDateString.isEmpty() == false)
    {
        KStarsDateTime startDate = KStarsDateTime::fromString( startDateString );
        if (startDate.isValid() )
            data()->changeDateTime( data()->geo()->LTtoUT( startDate ) );
        else
            data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );
    }
    else data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );

    // Initialize clock. If --paused is not in the comand line, look in options
    if ( startClock ) StartClockRunning =  Options::runClock();

    // Setup splash screen
    if ( doSplash ) {
        showSplash();
    } else {
        connect( m_KStarsData, SIGNAL( progressText(QString) ), m_KStarsData, SLOT( slotConsoleMessage(QString) ) );
    }

    //set up Dark color scheme for application windows
    //TODO: Move that to QML
    DarkPalette = QPalette(QColor("darkred"), QColor("darkred"));
    DarkPalette.setColor( QPalette::Normal, QPalette::Base, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Highlight, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::HighlightedText, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Base, QColor( 30, 10, 10 ) );
    //store original color scheme
    OriginalPalette = QApplication::palette();
    if( !m_KStarsData->initialize() ) return;
    datainitFinished();

#if ( __GLIBC__ >= 2 &&__GLIBC_MINOR__ >= 1  && !defined(__UCLIBC__) )
    qDebug() << "glibc >= 2.1 detected.  Using GNU extension sincos()";
#else
    qDebug() << "Did not find glibc >= 2.1.  Will use ANSI-compliant sin()/cos() functions.";
#endif
}

KStarsLite *KStarsLite::createInstance( bool doSplash, bool clockrunning, const QString &startDateString) {
    delete pinstance;
    // pinstance is set directly in constructor.
    new KStarsLite( doSplash, clockrunning, startDateString );
    Q_ASSERT( pinstance && "pinstance must be non NULL");
    return nullptr;
}

KStarsLite::~KStarsLite() {
    delete m_imgProvider;
}

void KStarsLite::fullUpdate() {
    m_KStarsData->setFullTimeUpdate();
    updateTime();

    m_SkyMapLite->forceUpdate();
}

void KStarsLite::updateTime( const bool automaticDSTchange ) {
    // Due to frequently use of this function save data and map pointers for speedup.
    // Save options and geo() to a pointer would not speedup because most of time options
    // and geo will accessed only one time.
    KStarsData *Data = data();
    // dms oldLST( Data->lst()->Degrees() );

    Data->updateTime( Data->geo(), automaticDSTchange );

    //We do this outside of kstarsdata just to get the coordinates
    //displayed in the infobox to update every second.
    //	if ( !Options::isTracking() && LST()->Degrees() > oldLST.Degrees() ) {
    //		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
    //		Map->focus()->setRA( Map->focus()->ra().Hours() + double( nSec )/3600. );
    //		if ( Options::useAltAz() ) Map->focus()->EquatorialToHorizontal( LST(), geo()->lat() );
    //		Map->showFocusCoords();
    //	}

    //If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
    //so that it can be ticked manually after each update, in order to make each time
    //step exactly equal to the timeScale setting.
    //Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
    //the skymap has been completely updated.
    if ( Data->clock()->isManualMode() && Data->clock()->isActive() ) {
        QTimer::singleShot( 0, Data->clock(), SLOT( manualTick() ) );
    }
}

void KStarsLite::writeConfig() {
    Options::self()->save();
    //Store current simulation time
    //Refer to // FIXME: Used in kstarsdcop.cpp only in kstarsdata.cpp
    //data()->StoredDate = data()->lt();
}

void KStarsLite::slotAboutToQuit()
{
    // Delete skymaplite. This required to run destructors and save
    // current state in the option.
    delete m_SkyMapLite;

    //Store Window geometry in Options object
    //Options::setWindowWidth( m_RootObject->width() );
    //Options::setWindowHeight( m_RootObject->height() );

    //explicitly save the colorscheme data to the config file
    data()->colorScheme()->saveToConfig();
    //synch the config file with the Config object
    writeConfig();
}

void KStarsLite::loadColorScheme( const QString &name ) {
    bool ok = data()->colorScheme()->load( name );
    QString filename = data()->colorScheme()->fileName();

    if ( ok ) {
        //set the application colors for the Night Vision scheme
        if ( Options::darkAppColors() == false && filename == "night.colors" )  {
            Options::setDarkAppColors( true );
            OriginalPalette = QApplication::palette();
            QApplication::setPalette( DarkPalette );
        }

        if ( Options::darkAppColors() && filename != "night.colors" ) {
            Options::setDarkAppColors( false );
            QApplication::setPalette( OriginalPalette );
        }

        Options::setColorSchemeFile( name );

        map()->forceUpdate();
    }
}