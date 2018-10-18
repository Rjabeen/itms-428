/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#include "commonstrings.h"

#include "importzmf.h"
#include "importzmfplugin.h"
#include "prefscontext.h"
#include "prefsfile.h"
#include "prefsmanager.h"
#include "scpage.h"
#include "scraction.h"
#include "scribuscore.h"
#include "undomanager.h"
#include "util_formats.h"

#include "ui/customfdialog.h"
#include "ui/scmwmenumanager.h"

int importzmf_getPluginAPIVersion()
{
	return PLUGIN_API_VERSION;
}

ScPlugin* importzmf_getPlugin()
{
	ImportZmfPlugin* plug = new ImportZmfPlugin();
	Q_CHECK_PTR(plug);
	return plug;
}

void importzmf_freePlugin(ScPlugin* plugin)
{
	ImportZmfPlugin* plug = dynamic_cast<ImportZmfPlugin*>(plugin);
	Q_ASSERT(plug);
	delete plug;
}

ImportZmfPlugin::ImportZmfPlugin() : LoadSavePlugin(),
	importAction(new ScrAction(ScrAction::DLL, QPixmap(), QPixmap(), "", QKeySequence(), this))
{
	// Set action info in languageChange, so we only have to do it in one
	// place. This includes registering file format support.
	registerFormats();
	languageChange();
}

void ImportZmfPlugin::languageChange()
{
	importAction->setText( tr("Import Zoner Draw..."));
	FileFormat* fmt = getFormatByExt("zmf");
	fmt->trName = tr("Zoner Draw");
	fmt->filter = tr("Zoner Draw (*.zmf *.ZMF)");
}

ImportZmfPlugin::~ImportZmfPlugin()
{
	unregisterAll();
}

const QString ImportZmfPlugin::fullTrName() const
{
	return QObject::tr("Zoner Draw Importer");
}


const ScActionPlugin::AboutData* ImportZmfPlugin::getAboutData() const
{
	AboutData* about = new AboutData;
	about->authors = "Franz Schmid <franz@scribus.info>";
	about->shortDescription = tr("Imports Zoner Draw Files");
	about->description = tr("Imports Zoner Draw 4-5 files into the current document, converting their vector data into Scribus objects.");
	about->license = "GPL";
	Q_CHECK_PTR(about);
	return about;
}

void ImportZmfPlugin::deleteAboutData(const AboutData* about) const
{
	Q_ASSERT(about);
	delete about;
}

void ImportZmfPlugin::registerFormats()
{
	FileFormat fmt(this);
	fmt.trName = tr("Zoner Draw");
	fmt.filter = tr("Zoner Draw (*.zmf *.ZMF)");
	fmt.formatId = 0;
	fmt.fileExtensions = QStringList() << "zmf";
	fmt.load = true;
	fmt.save = false;
	fmt.thumb = true;
	fmt.colorReading = true;
	fmt.mimeTypes = QStringList();
	fmt.priority = 64; // Priority
	registerFormat(fmt);
}

bool ImportZmfPlugin::fileSupported(QIODevice* /* file */, const QString & fileName) const
{
	return true;
}

bool ImportZmfPlugin::loadFile(const QString & fileName, const FileFormat &, int flags, int /*index*/)
{
	// There's only one format to handle, so we just call import(...)
	return import(fileName, flags);
}

bool ImportZmfPlugin::import(QString fileName, int flags)
{
	if (!checkFlags(flags))
		return false;
	if( fileName.isEmpty() )
	{
		flags |= lfInteractive;
		PrefsContext* prefs = PrefsManager::instance()->prefsFile->getPluginContext("importzmf");
		QString wdir = prefs->get("wdir", ".");
		CustomFDialog diaf(ScCore->primaryMainWindow(), wdir, QObject::tr("Open"), tr("All Supported Formats")+" (*.zmf *.ZMF);;All Files (*)");
		if (diaf.exec())
		{
			fileName = diaf.selectedFile();
			prefs->set("wdir", fileName.left(fileName.lastIndexOf("/")));
		}
		else
			return true;
	}
	m_Doc=ScCore->primaryMainWindow()->doc;
	UndoTransaction* activeTransaction = NULL;
	bool emptyDoc = (m_Doc == NULL);
	bool hasCurrentPage = (m_Doc && m_Doc->currentPage());
	TransactionSettings trSettings;
	trSettings.targetName   = hasCurrentPage ? m_Doc->currentPage()->getUName() : "";
	trSettings.targetPixmap = Um::IImageFrame;
	trSettings.actionName   = Um::ImportXfig;
	trSettings.description  = fileName;
	trSettings.actionPixmap = Um::IXFIG;
	if (emptyDoc || !(flags & lfInteractive) || !(flags & lfScripted))
		UndoManager::instance()->setUndoEnabled(false);
	if (UndoManager::undoEnabled())
		activeTransaction = new UndoTransaction(UndoManager::instance()->beginTransaction(trSettings));
	ZmfPlug *dia = new ZmfPlug(m_Doc, flags);
	Q_CHECK_PTR(dia);
	dia->import(fileName, trSettings, flags, !(flags & lfScripted));
	if (activeTransaction)
	{
		activeTransaction->commit();
		delete activeTransaction;
		activeTransaction = NULL;
	}
	if (emptyDoc || !(flags & lfInteractive) || !(flags & lfScripted))
		UndoManager::instance()->setUndoEnabled(true);
	delete dia;
	return true;
}

QImage ImportZmfPlugin::readThumbnail(const QString& fileName)
{
	if( fileName.isEmpty() )
		return QImage();
	UndoManager::instance()->setUndoEnabled(false);
	m_Doc = NULL;
	ZmfPlug *dia = new ZmfPlug(m_Doc, lfCreateThumbnail);
	Q_CHECK_PTR(dia);
	QImage ret = dia->readThumbnail(fileName);
	UndoManager::instance()->setUndoEnabled(true);
	delete dia;
	return ret;
}
