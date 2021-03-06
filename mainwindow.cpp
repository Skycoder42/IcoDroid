#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStandardPaths>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QImageReader>
#include <QScreen>
#ifdef Q_OS_OSX
#include <QMenuBar>
#endif
#include "dialogmaster.h"

static QStringList byteToStringList(const QByteArrayList &byteArrayList);

struct ScaleInfo {
	bool isChecked;
	QString subFolderSuffix;
	qreal scaleFactor;
};

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	settings(new QSettings(this)),
	mainModel(new PixmapModel(this)),
	previewDock(new IconViewDockWidget(this)),
	updateController(new QtAutoUpdater::UpdateController(this))//TODO "this" not ok because of conflict with sheets on mac ?!?
{
	this->initSettings();

	//setup basic ui
	this->ui->setupUi(this);
	this->addDockWidget(Qt::RightDockWidgetArea, this->previewDock);
	this->previewDock->hide();
	this->ui->basePathEdit->setDefaultDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
	this->ui->versionLabel->setText(QCoreApplication::applicationVersion());
	this->ui->tabWidget->setCurrentIndex(0);
	this->ui->loadViewListView->setModel(this->mainModel);
	this->ui->updatesLayout->replaceWidget(this->ui->checkUpdatesPlaceholder,
										   this->updateController->createUpdatePanel(this));
	this->ui->checkUpdatesPlaceholder->deleteLater();

#ifdef Q_OS_OSX
	//Mac actions...
	QMenu *menu = this->menuBar()->addMenu(tr("Help"));
	menu->addAction(this->updateController->getUpdateAction());
	QAction *aboutAction = new QAction(this->ui->aboutButton->text(), this);
	aboutAction->setMenuRole(QAction::AboutQtRole);
	connect(aboutAction, &QAction::triggered, qApp, &QApplication::aboutQt);
	menu->addAction(aboutAction);
#endif

	//setup actions
	this->ui->loadViewListView->addActions({
											   this->ui->actionAdd_File,
											   this->ui->actionRemove,
											   this->ui->actionClear_All
										   });
	this->ui->addIconButton->setDefaultAction(this->ui->actionAdd_File);
	this->ui->removeIconButton->addActions({
											   this->ui->actionRemove,
											   this->ui->actionClear_All
										   });
	this->ui->removeIconButton->setDefaultAction(this->ui->actionRemove);

	//setup icon sizes
	connect(this, &MainWindow::iconSizeChanged,
			this->ui->addIconButton, &QToolButton::setIconSize);
	connect(this, &MainWindow::iconSizeChanged,
			this->ui->removeIconButton, &QToolButton::setIconSize);
	this->setIconSize(QSize(24, 24) * (QApplication::primaryScreen()->logicalDotsPerInch() / 96.));//TODO experimental high dpi support?
	emit iconSizeChanged(this->iconSize());

	//restore gui
	this->settings->beginGroup(QStringLiteral("gui"));
	this->restoreGeometry(this->settings->value(QStringLiteral("geom")).toByteArray());
	this->restoreState(this->settings->value(QStringLiteral("state")).toByteArray());
	this->restoreDockWidget(this->previewDock);
	this->settings->endGroup();

	//restore dpi-states and ohter settings values
	this->settings->beginGroup(QStringLiteral("dpiRates"));
	this->ui->ldpiCheckBox->setChecked(this->settings->value(QStringLiteral("ldpi"), true).toBool());
	this->ui->mdpiCheckBox->setChecked(this->settings->value(QStringLiteral("mdpi"), true).toBool());
	this->ui->hdpiCheckBox->setChecked(this->settings->value(QStringLiteral("hdpi"), true).toBool());
	this->ui->xhdpiCheckBox->setChecked(this->settings->value(QStringLiteral("xhdpi"), true).toBool());
	this->ui->xxhdpiCheckBox->setChecked(this->settings->value(QStringLiteral("xxhdpi"), true).toBool());
	this->ui->xxxhdpiCheckBox->setChecked(this->settings->value(QStringLiteral("xxxhdpi"), true).toBool());
	this->settings->endGroup();
	this->ui->basePathEdit->setPath(this->settings->value(QStringLiteral("paths/savePath")).toString());

	//restore all custom profiles
	this->settings->beginGroup(QStringLiteral("templates"));
	this->ui->iconTypeComboBox->addItems(this->settings->childGroups());
	this->settings->endGroup();
	this->ui->iconTypeComboBox->setCurrentText(this->settings->value(QStringLiteral("recentProfile"), tr("Launcher Icon")).toString());

	//connections
	connect(this->ui->aboutButton, &QPushButton::clicked, qApp, &QApplication::aboutQt);
	connect(this->ui->previewCheckBox, &QCheckBox::clicked, this->previewDock, &IconViewDockWidget::setVisible);
	connect(this->previewDock, &IconViewDockWidget::visibilityChanged, this->ui->previewCheckBox, &QCheckBox::setChecked);
	this->ui->previewCheckBox->setChecked(this->previewDock->isVisible());
	connect(this->mainModel, &PixmapModel::rowsChanged, this, &MainWindow::rowsChanged);
	connect(this->ui->loadViewListView->selectionModel(), &QItemSelectionModel::currentChanged,
			this, &MainWindow::selectionChanged);
	connect(this->ui->actionClear_All, &QAction::triggered, this->mainModel, &PixmapModel::reset);

	//init functions
	this->on_iconTypeComboBox_activated(this->ui->iconTypeComboBox->currentText());
	this->updateController->scheduleUpdate(QDateTime::currentDateTime().addSecs(5));
}

MainWindow::~MainWindow()
{
	//save gui
	this->settings->beginGroup(QStringLiteral("gui"));
	this->settings->setValue(QStringLiteral("geom"), this->saveGeometry());
	this->settings->setValue(QStringLiteral("state"), this->saveState());
	this->settings->endGroup();

	//save dpi-states and profile
	this->settings->beginGroup(QStringLiteral("dpiRates"));
	this->settings->setValue(QStringLiteral("ldpi"), this->ui->ldpiCheckBox->isChecked());
	this->settings->setValue(QStringLiteral("mdpi"), this->ui->mdpiCheckBox->isChecked());
	this->settings->setValue(QStringLiteral("hdpi"), this->ui->hdpiCheckBox->isChecked());
	this->settings->setValue(QStringLiteral("xhdpi"), this->ui->xhdpiCheckBox->isChecked());
	this->settings->setValue(QStringLiteral("xxhdpi"), this->ui->xxhdpiCheckBox->isChecked());
	this->settings->setValue(QStringLiteral("xxxhdpi"), this->ui->xxxhdpiCheckBox->isChecked());
	this->settings->endGroup();
	if(this->ui->iconTypeComboBox->findText(this->ui->iconTypeComboBox->currentText()) >= 0)
		this->settings->setValue(QStringLiteral("recentProfile"), this->ui->iconTypeComboBox->currentText());

	delete this->ui;
}

void MainWindow::openFile(QString path)
{
	QIcon newIcon(path);
	if(!newIcon.isNull() && !newIcon.availableSizes().isEmpty()) {
		this->settings->setValue(QStringLiteral("paths/openPath"), QFileInfo(path).dir().absolutePath());
		this->mainModel->appendIcon(newIcon, path);
		this->ui->realFileLineEdit->setText(QFileInfo(path).baseName());
	} else {
		DialogMaster::critical(this, tr("Error"), tr("Unable to open icon \"%1\"").arg(path));
	}
}

void MainWindow::rowsChanged(int count)
{
	this->ui->createButton->setEnabled(count > 0);
}

void MainWindow::selectionChanged(const QModelIndex &current)
{
	this->previewDock->setPreviewIcon(this->mainModel->pixmap(current));
}

void MainWindow::fileSelected(const QStringList &selected)
{
	QFileDialog *dialog = qobject_cast<QFileDialog*>(QObject::sender());
	Q_ASSERT(dialog);

	this->settings->setValue(QStringLiteral("paths/openFilter"), dialog->selectedNameFilter());
	dialog->close();
	dialog->deleteLater();

	for(QString file : selected)
		this->openFile(file);
}

void MainWindow::on_actionAdd_File_triggered()
{
	this->settings->beginGroup(QStringLiteral("paths"));

	QFileDialog *dialog = new QFileDialog(this);
	DialogMaster::masterDialog(dialog);
	dialog->setWindowTitle(tr("Open Icon Archive"));
	dialog->setAcceptMode(QFileDialog::AcceptOpen);
	dialog->setFileMode(QFileDialog::ExistingFiles);
	dialog->setDirectory(this->settings->value(QStringLiteral("openPath")).toString());

	QStringList mTypes = byteToStringList(QImageReader::supportedMimeTypes());
	mTypes.append(QStringLiteral("application/octet-stream"));
	dialog->setMimeTypeFilters(mTypes);

	QString selFilter = this->settings->value(QStringLiteral("openFilter")).toString();
	qDebug() << selFilter;
	if(selFilter.isEmpty()) {
#if defined(Q_OS_WIN)
	dialog->selectMimeTypeFilter(QStringLiteral("image/vnd.microsoft.icon"));
#elif defined(Q_OS_OSX)
	dialog->selectMimeTypeFilter(QStringLiteral("image/x-icns"));
#else
	dialog->selectMimeTypeFilter(QStringLiteral("image/png"));
#endif
	} else
		dialog->selectNameFilter(selFilter);

	this->settings->endGroup();
	dialog->open(this, SLOT(fileSelected(QStringList)));
}

void MainWindow::on_actionRemove_triggered()
{
	this->mainModel->removeRow(this->ui->loadViewListView->currentIndex().row());
}

void MainWindow::on_iconTypeComboBox_activated(const QString &textName)
{
	this->settings->beginGroup(QStringLiteral("templates"));
	if(this->settings->childGroups().contains(textName)) {
		this->settings->beginGroup(textName);

		this->ui->baseSizeMdpiSpinBox->setValue(this->settings->value(QStringLiteral("baseSize")).toInt());
		this->ui->contentFolderComboBox->setCurrentText(this->settings->value(QStringLiteral("folderBaseName")).toString());

		this->settings->endGroup();
	}
	this->settings->endGroup();
}

void MainWindow::on_createButton_clicked()
{
	QString pathBase = this->ui->basePathEdit->path();
	QString subFolder = this->ui->contentFolderComboBox->currentText();
	QString realName = this->ui->realFileLineEdit->text();
	if(pathBase.isEmpty() || subFolder.isEmpty() || realName.isEmpty()) {
		DialogMaster::warning(this, tr("Warning"), tr("Please enter a valid base path, folder base and file name"));
		return;
	}

	this->settings->setValue(QStringLiteral("paths/savePath"), pathBase);
	this->settings->beginGroup(QStringLiteral("templates"));
	this->settings->beginGroup(this->ui->iconTypeComboBox->currentText());
	this->settings->setValue(QStringLiteral("baseSize"), this->ui->baseSizeMdpiSpinBox->value());
	this->settings->setValue(QStringLiteral("folderBaseName"), this->ui->contentFolderComboBox->currentText());
	this->settings->endGroup();
	this->settings->endGroup();
	this->ui->iconTypeComboBox->addItem(this->ui->iconTypeComboBox->currentText());

	QSize baseSize = {this->ui->baseSizeMdpiSpinBox->value(), this->ui->baseSizeMdpiSpinBox->value()};
	QVector<ScaleInfo> checkMap = {
		{this->ui->ldpiCheckBox->isChecked(), QStringLiteral("-ldpi"), 0.5},
		{this->ui->mdpiCheckBox->isChecked(), QStringLiteral("-mdpi"), 1.0},
		{this->ui->hdpiCheckBox->isChecked(), QStringLiteral("-hdpi"), 1.5},
		{this->ui->xhdpiCheckBox->isChecked(), QStringLiteral("-xhdpi"), 2.0},
		{this->ui->xxhdpiCheckBox->isChecked(), QStringLiteral("-xxhdpi"), 3.0},
		{this->ui->xxxhdpiCheckBox->isChecked(), QStringLiteral("-xxxhdpi"), 4.0}
	};
	for(int i = 0; i < 6; ++i) {
		if(checkMap[i].isChecked) {
			//create filepath
			QDir targetDir(pathBase);
			QString fullSubFolder = subFolder + checkMap[i].subFolderSuffix;
			if(!targetDir.mkpath(fullSubFolder)) {
				DialogMaster::critical(this, tr("Error"), tr("Failed to create subfolder \"%1\"").arg(fullSubFolder));
				return;
			}
			targetDir.cd(fullSubFolder);

			QSize realSize = baseSize * checkMap[i].scaleFactor;
			QPixmap targetPix = this->mainModel->findBestPixmap(realSize);
			if(targetPix.size() != realSize)
				targetPix = targetPix.scaled(realSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			QString finalPath = targetDir.absoluteFilePath(realName + QStringLiteral(".png"));
			if(!targetPix.save(finalPath, "png")) {
				DialogMaster::critical(this, tr("Error"), tr("Failed to create file \"%1\"").arg(finalPath));
				return;
			}
		}
	}

	DialogMaster::information(this, tr("Success"), tr("Icons successfully exported!"));
}

void MainWindow::initSettings()
{
	if(this->settings->value(QStringLiteral("isFirstStart"), true).toBool()) {
		this->settings->setValue(QStringLiteral("isFirstStart"), false);

		this->settings->beginGroup(QStringLiteral("paths"));
		this->settings->setValue(QStringLiteral("openPath"),
								 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
		this->settings->setValue(QStringLiteral("savePath"),
								 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
		this->settings->endGroup();

		this->settings->beginGroup(QStringLiteral("templates"));
		//Create "Launcher Icon"
		this->settings->beginGroup(tr("Launcher Icon"));
		this->settings->setValue(QStringLiteral("baseSize"), 48);
		this->settings->setValue(QStringLiteral("folderBaseName"), QStringLiteral("drawable"));
		this->settings->endGroup();
		//Create "Action Bar, Dialog, Tab"
		this->settings->beginGroup(tr("Action Bar, Dialog, Tab"));
		this->settings->setValue(QStringLiteral("baseSize"), 24);
		this->settings->setValue(QStringLiteral("folderBaseName"), QStringLiteral("drawable"));
		this->settings->endGroup();
		//Create "Context Menu"
		this->settings->beginGroup(tr("Context Menu"));
		this->settings->setValue(QStringLiteral("baseSize"), 16);
		this->settings->setValue(QStringLiteral("folderBaseName"), QStringLiteral("drawable"));
		this->settings->endGroup();
		//Create "Notification"
		this->settings->beginGroup(tr("Notification"));
		this->settings->setValue(QStringLiteral("baseSize"), 22);
		this->settings->setValue(QStringLiteral("folderBaseName"), QStringLiteral("drawable"));
		this->settings->endGroup();
		this->settings->endGroup();
	}
}

static QStringList byteToStringList(const QByteArrayList &byteArrayList)
{
	QStringList rList;
	for(QByteArray aValue : byteArrayList)
		rList += aValue;
	return rList;
}
