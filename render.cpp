
/****************************************************************************
**
** Copyright (C) 2007-2009 Kevin Clague. All rights reserved.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

/****************************************************************************
 *
 * This class encapsulates the external renderers.  For now, this means
 * only ldglite.
 *
 * Please see lpub.h for an overall description of how the files in LPub
 * make up the LPub program.
 *
 ***************************************************************************/

#include <QtGui>
#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include "render.h"
#include "resolution.h"
#include "meta.h"
#include "math.h"
#include "lpub.h"
#include "lpub_preferences.h"
#include "paths.h"

#ifndef __APPLE__
#include <windows.h>
#endif

Render *renderer;

LDGLite ldglite;
LDView  ldview;
L3P l3p;


//#define LduDistance 5729.57
#define CA "-ca0.01"
#define USE_ALPHA "+UA"

static double pi = 4*atan(1.0);
// the default camera distance for real size
static float LduDistance = 10.0/tan(0.005*pi/180);

QString fixupDirname(const QString &dirNameIn) {
#ifdef __APPLE__
	return dirNameIn;
#else
	long     length = 0;
    TCHAR*   buffer = NULL;
	LPCWSTR dirNameWin = dirNameIn.utf16();
// First obtain the size needed by passing NULL and 0.

    length = GetShortPathName(dirNameWin, NULL, 0);
    if (length == 0){
		qDebug() << "Couldn't get length of short path name, trying long path name\n";
		return dirNameIn;
	}
// Dynamically allocate the correct size 
// (terminating null char was included in length)

    buffer = new TCHAR[length];

// Now simply call again using same long path.

    length = GetShortPathName(dirNameWin, buffer, length);
    if (length == 0){
		qDebug() << "Couldn't get short path name, trying long path name\n";
		return dirNameIn;
	}

	QString dirNameOut = QString::fromWCharArray(buffer);
    
    delete [] buffer;
	return dirNameOut;
#endif
}

QString const Render::getRenderer()
{
  if (renderer == &ldglite) {
    return "LDGLite";
  } else if (renderer == &ldview){
    return "LDView";
  } else {
	  return "L3P";
  }
}

void Render::setRenderer(QString const &name)
{
  if (name == "LDGLite") {
    renderer = &ldglite;
  } else if (name == "LDView") {
    renderer = &ldview;
  } else {
	  renderer = &l3p;
  }
}

void clipImage(QString const &pngName){
	//printf("\n");
	QImage toClip(QDir::toNativeSeparators(pngName));
	QRect clipBox = toClip.rect();
	
	//printf("clipping %s from %d x %d at (%d,%d)\n",qPrintable(QDir::toNativeSeparators(pngName)),clipBox.width(),clipBox.height(),clipBox.x(),clipBox.y());
	
	int x,y;
	int initLeft = clipBox.left();
	int initTop = clipBox.top();
	int initRight = clipBox.right();
	int initBottom = clipBox.bottom();
	for(x = initLeft; x < initRight; x++){
		for(y = initTop; y < initBottom; y++){
			QRgb pixel = toClip.pixel(x, y);
			if(!toClip.valid(x,y) || !QColor::fromRgba(pixel).isValid()){
				//printf("something blew up when scanning at (%d,%d) - got %d %d\n",x,y,toClip.valid(x,y),QColor::fromRgba(pixel).isValid());
			}
			if ( pixel != 0){
				//printf("bumped into something at (%d,%d)\n",x,y);
				break;
			}
		}
		if (y != initBottom) {
			clipBox.setLeft(x);
			break;
		}
	}
	
	//printf("clipped to %d x %d at (%d,%d)\n",clipBox.width(),clipBox.height(),clipBox.x(),clipBox.y());
	
	
	initLeft = clipBox.left();
	for(x = initRight; x >= initLeft; x--){
		for(y = initTop; y < initBottom; y++){
			QRgb pixel = toClip.pixel(x, y);
			if(!toClip.valid(x,y) || !QColor::fromRgba(pixel).isValid()){
				//printf("something blew up when scanning at (%d,%d) - got %d %d\n",x,y,toClip.valid(x,y),QColor::fromRgba(pixel).isValid());
			}
			if ( pixel != 0){
				//printf("bumped into something at (%d,%d)\n",x,y);
				break;
			}
		}
		if (y != initBottom) {
			clipBox.setRight(x);
			break;
		}
	}
	
	//printf("clipped to %d x %d at (%d,%d)\n",clipBox.width(),clipBox.height(),clipBox.x(),clipBox.y());
	
	initRight = clipBox.right();
	for(y = initTop; y < initBottom; y++){
		for(x = initLeft; x < initRight; x++){
			QRgb pixel = toClip.pixel(x, y);
			if(!toClip.valid(x,y) || !QColor::fromRgba(pixel).isValid()){
				//printf("something blew up when scanning at (%d,%d) - got %d %d\n",x,y,toClip.valid(x,y),QColor::fromRgba(pixel).isValid());
			}
			if ( pixel != 0){
				//printf("bumped into something at (%d,%d)\n",x,y);
				break;
			}
		}
		if (x != initRight) {
			clipBox.setTop(y);
			break;
		}
	}
	
	//printf("clipped to %d x %d at (%d,%d)\n",clipBox.width(),clipBox.height(),clipBox.x(),clipBox.y());
	
	initTop = clipBox.top();
	for(y = initBottom; y >= initTop; y--){
		for(x = initLeft; x < initRight; x++){
			QRgb pixel = toClip.pixel(x, y);
			if(!toClip.valid(x,y) || !QColor::fromRgba(pixel).isValid()){
				//printf("something blew up when scanning at (%d,%d) - got %d %d\n",x,y,toClip.valid(x,y),QColor::fromRgba(pixel).isValid());
			}
			if ( pixel != 0){
				//printf("bumped into something at (%d,%d)\n",x,y);
				break;
			}
		}
		if (x != initRight) {
			clipBox.setBottom(y);
			break;
		}
	}
	
	//printf("clipped to %d x %d at (%d,%d)\n\n",clipBox.width(),clipBox.height(),clipBox.x(),clipBox.y());
	
	QImage clipped = toClip.copy(clipBox);
	//toClip.save(QDir::toNativeSeparators(pngName+"-orig.png"));
	clipped.save(QDir::toNativeSeparators(pngName));
}

// Shared calculations
float stdCameraDistance(Meta &meta, float scale) {
	float onexone;
	float factor;
	
	// Do the math in pixels
	
	onexone  = 20*meta.LPub.resolution.ldu(); // size of 1x1 in units
	onexone *= meta.LPub.resolution.value();  // size of 1x1 in pixels
	onexone *= scale;
	factor   = meta.LPub.page.size.valuePixels(0)/onexone; // in pixels;
	
	return factor*LduDistance;
}



/***************************************************************************
 *
 * The math for zoom factor.  1.0 is true size.
 *
 * 1 LDU is 1/64 of an inch
 *
 * LDGLite produces 72 DPI
 *
 * Camera angle is 0.01
 *
 * What distance do we need to put the camera, given a user chosen DPI,
 * to get zoom factor of 1.0?
 *
 **************************************************************************/


/***************************************************************************
 *
 * L3P renderer
 *
 **************************************************************************/
float L3P::cameraDistance(Meta &meta, float scale){
	return stdCameraDistance(meta, scale);
}

int L3P::renderCsi(
				   const QString     &addLine,
				   const QStringList &csiParts,
				   const QString     &pngName,
				   Meta        &meta){
	
	
	/* Create the CSI DAT file */
	QString ldrName;
	int rc;
	ldrName = QDir::currentPath() + "/" + Paths::tmpDir + "/csi.ldr";
	QString povName = ldrName +".pov";
	if ((rc = rotateParts(addLine,meta.rotStep, csiParts, ldrName)) < 0) {
		return rc;
	}
	
	/* determine camera distance */
	QStringList arguments;
	bool hasLGEO = Preferences::lgeoPath != "";
	
	int cd = cameraDistance(meta, meta.LPub.assem.modelScale.value());
	int width = meta.LPub.page.size.valuePixels(0);
	int height = meta.LPub.page.size.valuePixels(1);
	float ar = width/(float)height;
	
	QString cg = QString("-cg0.0,0.0,%1").arg(cd);
	QString car = QString("-car%1").arg(ar);
	QString ldd = QString("-ldd%1").arg(fixupDirname(QDir::toNativeSeparators(Preferences::ldrawPath)));
	arguments << CA;
	arguments << cg;
	arguments << "-ld";
	if(hasLGEO){
		QString lgd = QString("-lgd%1").arg(fixupDirname(QDir::toNativeSeparators(Preferences::lgeoPath)));
		arguments << "-lgeo";
		arguments << lgd;
	}
	arguments << car;
	arguments << "-o";
	arguments << ldd;
	QStringList list;
	list = meta.LPub.assem.l3pParms.value().split("\\s+");
	for (int i = 0; i < list.size(); i++) {
		if (list[i] != "" && list[i] != " ") {
			arguments << list[i];
		}
	}
	
	arguments << fixupDirname(QDir::toNativeSeparators(ldrName));
	arguments << fixupDirname(QDir::toNativeSeparators(povName));
	
	QProcess l3p;
	QStringList env = QProcess::systemEnvironment();
	l3p.setEnvironment(env);
	l3p.setWorkingDirectory(QDir::currentPath()+"/"+Paths::tmpDir);
	l3p.setStandardErrorFile(QDir::currentPath() + "/stderr");
	l3p.setStandardOutputFile(QDir::currentPath() + "/stdout");
	qDebug() << qPrintable(Preferences::l3pExe + " " + arguments.join(" ")) << "\n";
	l3p.start(Preferences::l3pExe,arguments);
	if ( ! l3p.waitForFinished(6*60*1000)) {
		if (l3p.exitCode() != 0) {
			QByteArray status = l3p.readAll();
			QString str;
			str.append(status);
			QMessageBox::warning(NULL,
								 QMessageBox::tr("LPub"),
								 QMessageBox::tr("L3P failed with code %1\n%2").arg(l3p.exitCode()) .arg(str));
			return -1;
		}
	}
	
	QStringList povArguments;
	QString O =QString("+O%1").arg(fixupDirname(QDir::toNativeSeparators(pngName)));
	QString I = QString("+I%1").arg(fixupDirname(QDir::toNativeSeparators(povName)));
	QString W = QString("+W%1").arg(width);
	QString H = QString("+H%1").arg(height);
	
	povArguments << I;
	povArguments << O;
	povArguments << W;
	povArguments << H;
	povArguments << USE_ALPHA;
	if(hasLGEO){
		QString lgeoLg = QString("+L%1").arg(fixupDirname(QDir::toNativeSeparators(Preferences::lgeoPath + "/lg")));
		QString lgeoAr = QString("+L%2").arg(fixupDirname(QDir::toNativeSeparators(Preferences::lgeoPath + "/ar")));
		povArguments << lgeoLg;
		povArguments << lgeoAr;
	}
#ifndef __APPLE__
	povArguments << "/EXIT";
#endif
	
	list = meta.LPub.assem.povrayParms.value().split("\\s+");
	for (int i = 0; i < list.size(); i++) {
		if (list[i] != "" && list[i] != " ") {
			povArguments << list[i];
		}
	}
	
	QProcess povray;
	QStringList povEnv = QProcess::systemEnvironment();
	povray.setEnvironment(povEnv);
	povray.setWorkingDirectory(QDir::currentPath()+"/"+Paths::tmpDir);
	povray.setStandardErrorFile(QDir::currentPath() + "/stderr-povray");
	povray.setStandardOutputFile(QDir::currentPath() + "/stdout-povray");
	qDebug() << qPrintable(Preferences::povrayExe + " " + povArguments.join(" ")) << "\n";
	povray.start(Preferences::povrayExe,povArguments);
	if ( ! povray.waitForFinished(6*60*1000)) {
		if (povray.exitCode() != 0) {
			QByteArray status = povray.readAll();
			QString str;
			str.append(status);
			QMessageBox::warning(NULL,
								 QMessageBox::tr("LPub"),
								 QMessageBox::tr("POV-RAY failed with code %1\n%2").arg(povray.exitCode()) .arg(str));
			return -1;
		}
	}
	
	clipImage(pngName);

	return 0;
	
}

int L3P::renderPli(const QString &ldrName,
				   const QString &pngName,
				   Meta    &meta,
				   bool     bom){
	
	QString povName = ldrName +".pov";
	
	int width  = meta.LPub.page.size.valuePixels(0);
	int height = meta.LPub.page.size.valuePixels(1);
	float ar = width/(float)height;
	
	/* determine camera distance */
	
	PliMeta &pliMeta = bom ? meta.LPub.bom : meta.LPub.pli;
	
	int cd = cameraDistance(meta,pliMeta.modelScale.value());
	
	QString cg = QString("-cg%1,%2,%3") .arg(pliMeta.angle.value(0))
	.arg(pliMeta.angle.value(1))
	.arg(cd);
	
	QString car = QString("-car%1").arg(ar);
	QString ldd = QString("-ldd%1").arg(fixupDirname(QDir::toNativeSeparators(Preferences::ldrawPath)));
	QStringList arguments;
	bool hasLGEO = Preferences::lgeoPath != "";
	
	arguments << CA;
	arguments << cg;
	arguments << "-ld";
	if(hasLGEO){
		QString lgd = QString("-lgd%1").arg(fixupDirname(QDir::toNativeSeparators(Preferences::lgeoPath)));
		arguments << "-lgeo";
		arguments << lgd;
	}
	arguments << car;
	arguments << "-o";
	arguments << ldd;
	
	QStringList list;
	list = meta.LPub.assem.l3pParms.value().split("\\s+");
	for (int i = 0; i < list.size(); i++) {
		if (list[i] != "" && list[i] != " ") {
			arguments << list[i];
		}
	}
	
	arguments << fixupDirname(QDir::toNativeSeparators(ldrName));
	arguments << fixupDirname(QDir::toNativeSeparators(povName));
	
	QProcess    l3p;
	QStringList env = QProcess::systemEnvironment();
	l3p.setEnvironment(env);
	l3p.setWorkingDirectory(QDir::currentPath());
	l3p.setStandardErrorFile(QDir::currentPath() + "/stderr");
	l3p.setStandardOutputFile(QDir::currentPath() + "/stdout");
	qDebug() << qPrintable(Preferences::l3pExe + " " + arguments.join(" ")) << "\n";
	l3p.start(Preferences::l3pExe,arguments);
	if (! l3p.waitForFinished()) {
		if (l3p.exitCode()) {
			QByteArray status = l3p.readAll();
			QString str;
			str.append(status);
			QMessageBox::warning(NULL,
								 QMessageBox::tr("LPub"),
								 QMessageBox::tr("L3P failed\n%1") .arg(str));
			return -1;
		}
	}
	
	QStringList povArguments;
	QString O =QString("+O%1").arg(fixupDirname(QDir::toNativeSeparators(pngName)));
	QString I = QString("+I%1").arg(fixupDirname(QDir::toNativeSeparators(povName)));
	QString W = QString("+W%1").arg(width);
	QString H = QString("+H%1").arg(height);
	
	povArguments << I;
	povArguments << O;
	povArguments << W;
	povArguments << H;
	povArguments << USE_ALPHA;
	if(hasLGEO){
		QString lgeoLg = QString("+L%1").arg(fixupDirname(QDir::toNativeSeparators(Preferences::lgeoPath + "/lg")));
		QString lgeoAr = QString("+L%2").arg(fixupDirname(QDir::toNativeSeparators(Preferences::lgeoPath + "/ar")));
		povArguments << lgeoLg;
		povArguments << lgeoAr;
	}
#ifndef __APPLE__
	povArguments << "/EXIT";
#endif
	
	list = meta.LPub.assem.povrayParms.value().split("\\s+");
	for (int i = 0; i < list.size(); i++) {
		if (list[i] != "" && list[i] != " ") {
			povArguments << list[i];
		}
	}
	
	QProcess povray;
	QStringList povEnv = QProcess::systemEnvironment();
	povray.setEnvironment(povEnv);
	povray.setWorkingDirectory(QDir::currentPath()+"/"+Paths::tmpDir);
	povray.setStandardErrorFile(QDir::currentPath() + "/stderr-povray");
	povray.setStandardOutputFile(QDir::currentPath() + "/stdout-povray");
	qDebug() << qPrintable(Preferences::povrayExe + " " + povArguments.join(" ")) << "\n";
	povray.start(Preferences::povrayExe,povArguments);
	if ( ! povray.waitForFinished(6*60*1000)) {
		if (povray.exitCode() != 0) {
			QByteArray status = povray.readAll();
			QString str;
			str.append(status);
			QMessageBox::warning(NULL,
								 QMessageBox::tr("LPub"),
								 QMessageBox::tr("POV-RAY failed\n%1") .arg(str));
			return -1;
		}
	}
	
	clipImage(pngName);
	
	return 0;

	
}


/***************************************************************************
 *
 * LDGLite renderer
 *
 **************************************************************************/

float LDGLite::cameraDistance(
  Meta &meta,
  float scale)
{
	return stdCameraDistance(meta,scale);
}

int LDGLite::renderCsi(
  const QString     &addLine,
  const QStringList &csiParts,
  const QString     &pngName,
        Meta        &meta)
{
	/* Create the CSI DAT file */
	QString ldrName;
	int rc;
	ldrName = QDir::currentPath() + "/" + Paths::tmpDir + "/csi.ldr";
	if ((rc = rotateParts(addLine,meta.rotStep, csiParts, ldrName)) < 0) {
		return rc;
	}

  /* determine camera distance */
  
  QStringList arguments;

  int cd = cameraDistance(meta,meta.LPub.assem.modelScale.value());

  int width = meta.LPub.page.size.valuePixels(0);
  int height = meta.LPub.page.size.valuePixels(1);

  QString v  = QString("-v%1,%2")   .arg(width)
                                    .arg(height);
  QString o  = QString("-o0,-%1")   .arg(height/6);
  QString mf = QString("-mF%1")     .arg(pngName);
  
  int lineThickness = resolution()/150+0.5;
  if (lineThickness == 0) {
    lineThickness = 1;
  }
                                    // ldglite always deals in 72 DPI
  QString w  = QString("-W%1")      .arg(lineThickness);

   QString cg = QString("-cg0.0,0.0,%1") .arg(cd);

  arguments << "-l3";
  arguments << "-i2";
  arguments << CA;
  arguments << cg;
  arguments << "-J";
  arguments << v;
  arguments << o;
  arguments << w;
  arguments << "-q";

  QStringList list;
  list = meta.LPub.assem.ldgliteParms.value().split("\\s+");
  for (int i = 0; i < list.size(); i++) {
    if (list[i] != "" && list[i] != " ") {
      arguments << list[i];
    }
  }

  arguments << mf;
  arguments << ldrName;
  
  QProcess    ldglite;
  QStringList env = QProcess::systemEnvironment();
  env << "LDRAWDIR=" + Preferences::ldrawPath;
  ldglite.setEnvironment(env);
  ldglite.setWorkingDirectory(QDir::currentPath()+"/"+Paths::tmpDir);
  ldglite.setStandardErrorFile(QDir::currentPath() + "/stderr");
  ldglite.setStandardOutputFile(QDir::currentPath() + "/stdout");
  ldglite.start(Preferences::ldgliteExe,arguments);
  if ( ! ldglite.waitForFinished(6*60*1000)) {
    if (ldglite.exitCode() != 0) {
      QByteArray status = ldglite.readAll();
      QString str;
      str.append(status);
      QMessageBox::warning(NULL,
                           QMessageBox::tr("LPub"),
                           QMessageBox::tr("LDGlite failed\n%1") .arg(str));
      return -1;
    }
  }
  //QFile::remove(ldrName);
  return 0;
}

  
int LDGLite::renderPli(
  const QString &ldrName,
  const QString &pngName,
  Meta    &meta,
  bool     bom)
{
  int width  = meta.LPub.page.size.valuePixels(0);
  int height = meta.LPub.page.size.valuePixels(1);
  
  /* determine camera distance */

  PliMeta &pliMeta = bom ? meta.LPub.bom : meta.LPub.pli;

  int cd = cameraDistance(meta,pliMeta.modelScale.value());

  QString cg = QString("-cg%1,%2,%3") .arg(pliMeta.angle.value(0))
                                      .arg(pliMeta.angle.value(1))
                                      .arg(cd);

  QString v  = QString("-v%1,%2")   .arg(width)
                                    .arg(height);
  QString o  = QString("-o0,-%1")   .arg(height/6);
  QString mf = QString("-mF%1")     .arg(pngName);
                                    // ldglite always deals in 72 DPI
  QString w  = QString("-W%1")      .arg(int(resolution()/72.0+0.5));

  QStringList arguments;
  arguments << "-l3";
  arguments << "-i2";
  arguments << CA;
  arguments << cg;
  arguments << "-J";
  arguments << v;
  arguments << o;
  arguments << w;
  arguments << "-q";

  QStringList list;
  list = meta.LPub.pli.ldgliteParms.value().split("\\s+");
  for (int i = 0; i < list.size(); i++) {
	  if (list[i] != "" && list[i] != " ") {
      arguments << list[i];
	  }
  }
  arguments << mf;
  arguments << ldrName;
  
  QProcess    ldglite;
  QStringList env = QProcess::systemEnvironment();
  env << "LDRAWDIR=" + Preferences::ldrawPath;
  ldglite.setEnvironment(env);  
  ldglite.setWorkingDirectory(QDir::currentPath());
  ldglite.setStandardErrorFile(QDir::currentPath() + "/stderr");
  ldglite.setStandardOutputFile(QDir::currentPath() + "/stdout");
  ldglite.start(Preferences::ldgliteExe,arguments);
  if (! ldglite.waitForFinished()) {
    if (ldglite.exitCode()) {
      QByteArray status = ldglite.readAll();
      QString str;
      str.append(status);
      QMessageBox::warning(NULL,
                           QMessageBox::tr("LPub"),
                           QMessageBox::tr("LDGlite failed\n%1") .arg(str));
      return -1;
    }
  }
  return 0;
}


/***************************************************************************
 *
 * LDView renderer
 *                                  6x6                    5990
 *      LDView               LDView    LDGLite       LDView
 * 0.1    8x5     8x6         32x14    40x19  0.25  216x150    276x191  0.28
 * 0.2   14x10   16x10                              430x298    552x381
 * 0.3   20x14   20x15                              644x466    824x571
 * 0.4   28x18   28x19                              859x594   1100x762
 * 0.5   34x22   36x22                             1074x744   1376x949  0.28
 * 0.6   40x27   40x28                             1288x892
 * 0.7   46x31   48x32                            1502x1040
 * 0.8   54x35   56x37                          
 * 0.9   60x40   60x41
 * 1.0   66x44   68x46       310x135  400x175 0.29 
 * 1.1   72x48
 * 1.2   80x53
 * 1.3   86x57
 * 1.4   92x61
 * 1.5   99x66
 * 1.6  106x70
 * 2.0  132x87  132x90       620x270  796x348 0.28
 * 3.0  197x131 200x134      930x404 1169x522
 * 4.0  262x174 268x178     1238x539 1592x697 0.29
 * 5.0  328x217 332x223     1548x673
 * 
 *
 **************************************************************************/

float LDView::cameraDistance(
  Meta &meta,
  float scale)
{
	return stdCameraDistance(meta, scale)*0.775;
}

int LDView::renderCsi(
  const QString     &addLine,
  const QStringList &csiParts,
  const QString     &pngName,
        Meta        &meta)
{
	/* Create the CSI DAT file */
	QString ldrName;
	int rc;
	ldrName = QDir::currentPath() + "/" + Paths::tmpDir + "/csi.ldr";
	if ((rc = rotateParts(addLine,meta.rotStep, csiParts, ldrName)) < 0) {
		return rc;
	}
	

  /* determine camera distance */
  
  QStringList arguments;

  int cd = cameraDistance(meta,meta.LPub.assem.modelScale.value())*1700/1000;
  int width = meta.LPub.page.size.valuePixels(0);
  int height = meta.LPub.page.size.valuePixels(1);

  QString w  = QString("-SaveWidth=%1") .arg(width);
  QString h  = QString("-SaveHeight=%1") .arg(height);
  QString s  = QString("-SaveSnapShot=%1") .arg(pngName);

  QString cg = QString("-cg0.0,0.0,%1") .arg(cd);

  arguments << CA;
  arguments << cg;
  arguments << "-SaveAlpha=1";
  arguments << "-AutoCrop=1";
  arguments << "-ShowHighlightLines=1";
  arguments << "-ConditionalHighlights=1";
  arguments << "-SaveZoomToFit=0";
  arguments << "-SubduedLighting=1";
  arguments << "-UseSpecular=0";
  arguments << "-LightVector=0,1,1";
  arguments << "-SaveActualSize=0";
  arguments << w;
  arguments << h;
  arguments << s;

  QStringList list;
  list = meta.LPub.assem.ldviewParms.value().split("\\s+");
  for (int i = 0; i < list.size(); i++) {
    if (list[i] != "" && list[i] != " ") {
      arguments << list[i];
    }
  }
  arguments << ldrName;
  
  QProcess    ldview;
  ldview.setEnvironment(QProcess::systemEnvironment());
  ldview.setWorkingDirectory(QDir::currentPath()+"/"+Paths::tmpDir);
  ldview.start(Preferences::ldviewExe,arguments);

  if ( ! ldview.waitForFinished(6*60*1000)) {
    if (ldview.exitCode() != 0 || 1) {
      QByteArray status = ldview.readAll();
      QString str;
      str.append(status);
      QMessageBox::warning(NULL,
                           QMessageBox::tr("LPub"),
                           QMessageBox::tr("LDView failed\n%1") .arg(str));
      return -1;
    }
  }
  //QFile::remove(ldrName);
  return 0;
}

  
int LDView::renderPli(
  const QString &ldrName,
  const QString &pngName,
  Meta    &meta,
  bool     bom)
{
  int width  = meta.LPub.page.size.valuePixels(0);
  int height = meta.LPub.page.size.valuePixels(1);
  
  QFileInfo fileInfo(ldrName);
  
  if ( ! fileInfo.exists()) {
    return -1;
  }

  /* determine camera distance */

  PliMeta &pliMeta = bom ? meta.LPub.bom : meta.LPub.pli;
  
  int cd = cameraDistance(meta,pliMeta.modelScale.value())*1700/1000;

  QString cg = QString("-cg%1,%2,%3") .arg(pliMeta.angle.value(0))
                                      .arg(pliMeta.angle.value(1))
                                      .arg(cd);
  QString w  = QString("-SaveWidth=%1")  .arg(width);
  QString h  = QString("-SaveHeight=%1") .arg(height);
  QString s  = QString("-SaveSnapShot=%1") .arg(pngName);

  QStringList arguments;
  arguments << CA;
  arguments << cg;
  arguments << "-SaveAlpha=1";
  arguments << "-AutoCrop=1";
  arguments << "-ShowHighlightLines=1";
  arguments << "-ConditionalHighlights=1";
  arguments << "-SaveZoomToFit=0";
  arguments << "-SubduedLighting=1";
  arguments << "-UseSpecular=0";
  arguments << "-LightVector=0,1,1";
  arguments << "-SaveActualSize=0";
  arguments << w;
  arguments << h;
  arguments << s;

  QStringList list;
  list = meta.LPub.pli.ldviewParms.value().split("\\s+");
  for (int i = 0; i < list.size(); i++) {
    if (list[i] != "" && list[i] != " ") {
      arguments << list[i];
    }
  }
  arguments << ldrName;

  QProcess    ldview;
  ldview.setEnvironment(QProcess::systemEnvironment());
  ldview.setWorkingDirectory(QDir::currentPath());
  ldview.start(Preferences::ldviewExe,arguments);
  if ( ! ldview.waitForFinished()) {
    if (ldview.exitCode() != 0) {
      QByteArray status = ldview.readAll();
      QString str;
      str.append(status);
      QMessageBox::warning(NULL,
                           QMessageBox::tr("LPub"),
                           QMessageBox::tr("LDView failed\n%1") .arg(str));
      return -1;
    }
  }
  return 0;
}
