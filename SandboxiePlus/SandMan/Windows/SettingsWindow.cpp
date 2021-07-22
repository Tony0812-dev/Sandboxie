#include "stdafx.h"
#include "SettingsWindow.h"
#include "SandMan.h"
#include "../MiscHelpers/Common/Settings.h"
#include "Helpers/WinAdmin.h"
#include "../QSbieAPI/Sandboxie/SbieTemplates.h"
#include "../QSbieAPI/SbieUtils.h"


QSize CustomTabStyle::sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget) const {
	QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
	if (type == QStyle::CT_TabBarTab) {
		s.transpose();
		if(theGUI->m_DarkTheme)
			s.setHeight(s.height() * 13 / 10);
		else
			s.setHeight(s.height() * 15 / 10);
		s.setWidth(s.width() * 11 / 10); // for the the icon
	}
	return s;
}

void CustomTabStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
	if (element == CE_TabBarTabLabel) {
		if (const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
			QStyleOptionTab opt(*tab);
			opt.shape = QTabBar::RoundedNorth;
			//opt.iconSize = QSize(32, 32);
			opt.iconSize = QSize(24, 24);
			QProxyStyle::drawControl(element, &opt, painter, widget);
			return;
		}
	}
	QProxyStyle::drawControl(element, option, painter, widget);
}



int CSettingsWindow__Chk2Int(Qt::CheckState state)
{
	switch (state) {
	case Qt::Unchecked: return 0;
	case Qt::Checked: return 1;
	default:
	case Qt::PartiallyChecked: return 2;
	}
}

Qt::CheckState CSettingsWindow__Int2Chk(int state)
{
	switch (state) {
	case 0: return Qt::Unchecked;
	case 1: return Qt::Checked;
	default:
	case 2: return Qt::PartiallyChecked;
	}
}

CSettingsWindow::CSettingsWindow(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	this->setWindowTitle(tr("Sandboxie Plus - Settings"));

	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	setWindowFlags(flags);

	bool bAlwaysOnTop = theConf->GetBool("Options/AlwaysOnTop", false);
	this->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);


	ui.tabs->setTabPosition(QTabWidget::West);
	ui.tabs->tabBar()->setStyle(new CustomTabStyle(ui.tabs->tabBar()->style()));

	ui.tabs->setTabIcon(0, CSandMan::GetIcon("Options"));
	ui.tabs->setTabIcon(1, CSandMan::GetIcon("Advanced"));
	ui.tabs->setTabIcon(2, CSandMan::GetIcon("Ampel"));
	ui.tabs->setTabIcon(3, CSandMan::GetIcon("Compatibility"));
	ui.tabs->setTabIcon(4, CSandMan::GetIcon("Support"));


	ui.tabs->setCurrentIndex(0);

	ui.uiLang->addItem(tr("Auto Detection"), "");
	ui.uiLang->addItem("International English", "en");
	QDir langDir(QApplication::applicationDirPath() + "/translations/");
	foreach(const QString& langFile, langDir.entryList(QStringList("sandman_*.qm"), QDir::Files))
	{
		QString Code = langFile.mid(8, langFile.length() - 8 - 3);
		QLocale Locale(Code);
		QString Lang = Locale.nativeLanguageName();
		ui.uiLang->addItem(Lang, Code);
	}

	LoadSettings();

	connect(ui.chkShowTray, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));
	//connect(ui.chkUseCycles, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));

	
	m_WarnProgsChanged = false;

	connect(ui.chkPassRequired, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));
	connect(ui.btnSetPassword, SIGNAL(clicked(bool)), this, SLOT(OnSetPassword()));

	connect(ui.chkStartBlock, SIGNAL(stateChanged(int)), this, SLOT(OnWarnChanged()));

	connect(ui.chkStartBlockMsg, SIGNAL(stateChanged(int)), this, SLOT(OnWarnChanged()));
	connect(ui.btnAddWarnProg, SIGNAL(clicked(bool)), this, SLOT(OnAddWarnProg()));
	connect(ui.btnAddWarnFolder, SIGNAL(clicked(bool)), this, SLOT(OnAddWarnFolder()));
	connect(ui.btnDelWarnProg, SIGNAL(clicked(bool)), this, SLOT(OnDelWarnProg()));

	connect(ui.btnBrowse, SIGNAL(clicked(bool)), this, SLOT(OnBrowse()));

	connect(ui.chkAutoRoot, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));


	connect(ui.btnAddCompat, SIGNAL(clicked(bool)), this, SLOT(OnAddCompat()));
	connect(ui.btnDelCompat, SIGNAL(clicked(bool)), this, SLOT(OnDelCompat()));
	m_CompatLoaded = 0;
	m_CompatChanged = false;
	ui.chkNoCompat->setChecked(!theConf->GetBool("Options/AutoRunSoftCompat", true));

	connect(ui.treeCompat, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnTemplateClicked(QTreeWidgetItem*, int)));

	connect(ui.lblSupport, SIGNAL(linkActivated(const QString&)), this, SLOT(OnSupport(const QString&)));


	connect(ui.tabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab()));

	connect(ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked(bool)), this, SLOT(ok()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("SettingsWindow/Window_Geometry"));
}

CSettingsWindow::~CSettingsWindow()
{
	theConf->SetBlob("SettingsWindow/Window_Geometry",saveGeometry());
}

void CSettingsWindow::OnSupport(const QString& url)
{
	QDesktopServices::openUrl(url);
}

void CSettingsWindow::showCompat()
{
	m_CompatLoaded = 2;
	ui.tabs->setCurrentWidget(ui.tabCompat);
	show();
}

void CSettingsWindow::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

Qt::CheckState CSettingsWindow__IsContextMenu()
{
	QString cmd = CSbieUtils::GetContextMenuStartCmd();
	if (cmd.contains("sandman.exe", Qt::CaseInsensitive)) 
		return Qt::Checked; // set up and sandman
	if (!cmd.isEmpty()) // ... probably sbiectrl.exe
		return Qt::PartiallyChecked; 
	return Qt::Unchecked; // not set up
}

void CSettingsWindow::LoadSettings()
{
	ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));

	ui.chkAutoStart->setChecked(IsAutorunEnabled());
	if (theAPI->GetUserSettings()->GetBool("SbieCtrl_EnableAutoStart", true)) {
		if (theAPI->GetUserSettings()->GetText("SbieCtrl_AutoStartAgent", "") != "SandMan.exe")
			ui.chkSvcStart->setCheckState(Qt::PartiallyChecked);
		else
			ui.chkSvcStart->setChecked(true);
	} else
		ui.chkSvcStart->setChecked(false);

	ui.chkShellMenu->setCheckState(CSettingsWindow__IsContextMenu());
	ui.chkAlwaysDefault->setChecked(theConf->GetBool("Options/RunInDefaultBox", false));

	ui.chkDarkTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseDarkTheme", 2)));

	ui.chkNotifications->setChecked(theConf->GetBool("Options/ShowNotifications", true));

	ui.chkSandboxUrls->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/OpenUrlsSandboxed", 2)));

	ui.chkShowRecovery->setChecked(theConf->GetBool("Options/ShowRecovery", false));

	ui.chkPanic->setChecked(theConf->GetBool("Options/EnablePanicKey", false));
	ui.keyPanic->setKeySequence(QKeySequence(theConf->GetString("Options/PanicKeySequence", "Ctrl+Alt+Cancel")));

	ui.chkWatchConfig->setChecked(theConf->GetBool("Options/WatchIni", true));

	ui.onClose->addItem(tr("Close to Tray"), "ToTray");
	ui.onClose->addItem(tr("Prompt before Close"), "Prompt");
	ui.onClose->addItem(tr("Close"), "Close");
	ui.onClose->setCurrentIndex(ui.onClose->findData(theConf->GetString("Options/OnClose", "ToTray")));

	ui.chkShowTray->setChecked(theConf->GetBool("Options/ShowSysTray", true));


	if (theAPI->IsConnected())
	{
		QString FileRootPath_Default = "\\??\\%SystemDrive%\\Sandbox\\%USER%\\%SANDBOX%";
		QString KeyRootPath_Default  = "\\REGISTRY\\USER\\Sandbox_%USER%_%SANDBOX%";
		QString IpcRootPath_Default  = "\\Sandbox\\%USER%\\%SANDBOX%\\Session_%SESSION%";

		ui.fileRoot->setText(theAPI->GetGlobalSettings()->GetText("FileRootPath", FileRootPath_Default));
		ui.chkSeparateUserFolders->setChecked(theAPI->GetGlobalSettings()->GetBool("SeparateUserFolders", true));
		ui.regRoot->setText(theAPI->GetGlobalSettings()->GetText("KeyRootPath", KeyRootPath_Default));
		ui.ipcRoot->setText(theAPI->GetGlobalSettings()->GetText("IpcRootPath", IpcRootPath_Default));


		ui.chkAdminOnly->setChecked(theAPI->GetGlobalSettings()->GetBool("EditAdminOnly", false));
		ui.chkPassRequired->setChecked(!theAPI->GetGlobalSettings()->GetText("EditPassword", "").isEmpty());
		ui.chkAdminOnlyFP->setChecked(theAPI->GetGlobalSettings()->GetBool("ForceDisableAdminOnly", false));
		ui.chkClearPass->setChecked(theAPI->GetGlobalSettings()->GetBool("ForgetPassword", false));

		ui.chkStartBlock->setChecked(theAPI->GetGlobalSettings()->GetBool("StartRunAlertDenied", false));
		ui.chkStartBlockMsg->setChecked(theAPI->GetGlobalSettings()->GetBool("NotifyStartRunAccessDenied", true));
		
		foreach(const QString& Value, theAPI->GetGlobalSettings()->GetTextList("AlertProcess", false))
			AddWarnEntry(Value, 1);
		
		foreach(const QString& Value, theAPI->GetGlobalSettings()->GetTextList("AlertFolder", false))
			AddWarnEntry(Value, 2);
	}
	else
	{
		ui.fileRoot->setEnabled(false);
		ui.chkSeparateUserFolders->setEnabled(false);
		ui.regRoot->setEnabled(false);
		ui.ipcRoot->setEnabled(false);
		ui.chkAdminOnly->setEnabled(false);
		ui.chkPassRequired->setEnabled(false);
		ui.chkAdminOnlyFP->setEnabled(false);
		ui.chkClearPass->setEnabled(false);
		ui.btnSetPassword->setEnabled(false);
		ui.treeWarnProgs->setEnabled(false);
		ui.btnAddWarnProg->setEnabled(false);
		ui.btnDelWarnProg->setEnabled(false);
		ui.treeCompat->setEnabled(false);
		ui.btnAddCompat->setEnabled(false);
		ui.btnDelCompat->setEnabled(false);
	}


	int PortableRootDir = theConf->GetInt("Options/PortableRootDir", -1);
	if (PortableRootDir != -1 && theConf->IsPortable())
		ui.chkAutoRoot->setChecked(PortableRootDir == 0 ? Qt::Unchecked : Qt::Checked);
	else
		ui.chkAutoRoot->setVisible(false);


	ui.chkAutoUpdate->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/CheckForUpdates", 2)));
	ui.chkAutoInstall->setVisible(false); // todo implement smart auto updater

	OnChange();
}

void CSettingsWindow::SaveSettings()
{
	theConf->SetValue("Options/UiLanguage", ui.uiLang->currentData());

	theConf->SetValue("Options/UseDarkTheme", CSettingsWindow__Chk2Int(ui.chkDarkTheme->checkState()));

	AutorunEnable(ui.chkAutoStart->isChecked());

	if (ui.chkSvcStart->checkState() == Qt::Checked) {
		theAPI->GetUserSettings()->SetBool("SbieCtrl_EnableAutoStart", true);
		theAPI->GetUserSettings()->SetText("SbieCtrl_AutoStartAgent", "SandMan.exe");
	} else if (ui.chkSvcStart->checkState() == Qt::Unchecked)
		theAPI->GetUserSettings()->SetBool("SbieCtrl_EnableAutoStart", false);

	if (ui.chkShellMenu->checkState() != CSettingsWindow__IsContextMenu())
	{
		if (ui.chkShellMenu->isChecked()) {
			CSbieUtils::AddContextMenu(QApplication::applicationDirPath().replace("/", "\\") + "\\SandMan.exe",
				QApplication::applicationDirPath().replace("/", "\\") + "\\Start.exe");
		} else
			CSbieUtils::RemoveContextMenu();
	}
	theConf->SetValue("Options/RunInDefaultBox", ui.chkAlwaysDefault->isChecked());

	theConf->SetValue("Options/ShowNotifications", ui.chkNotifications->isChecked());

	theConf->SetValue("Options/OpenUrlsSandboxed", CSettingsWindow__Chk2Int(ui.chkSandboxUrls->checkState()));

	theConf->SetValue("Options/ShowRecovery", ui.chkShowRecovery->isChecked());

	theConf->SetValue("Options/EnablePanicKey", ui.chkPanic->isChecked());
	theConf->SetValue("Options/PanicKeySequence", ui.keyPanic->keySequence().toString());
	
	theConf->SetValue("Options/WatchIni", ui.chkWatchConfig->isChecked());

	theConf->SetValue("Options/OnClose", ui.onClose->currentData());

	theConf->SetValue("Options/ShowSysTray", ui.chkShowTray->isChecked());


	if (theAPI->IsConnected())
	{
		if (ui.fileRoot->text().isEmpty())
			theAPI->GetGlobalSettings()->DelValue("FileRootPath"); //ui.fileRoot->setText("\\??\\%SystemDrive%\\Sandbox\\%USER%\\%SANDBOX%");
		else
			theAPI->GetGlobalSettings()->SetText("FileRootPath", ui.fileRoot->text());
		theAPI->GetGlobalSettings()->SetBool("SeparateUserFolders", ui.chkSeparateUserFolders->isChecked());

		if (ui.regRoot->text().isEmpty())
			theAPI->GetGlobalSettings()->DelValue("KeyRootPath"); //ui.regRoot->setText("\\REGISTRY\\USER\\Sandbox_%USER%_%SANDBOX%");
		else
			theAPI->GetGlobalSettings()->SetText("KeyRootPath", ui.regRoot->text());

		if (ui.ipcRoot->text().isEmpty())
			theAPI->GetGlobalSettings()->DelValue("IpcRootPath"); //ui.ipcRoot->setText("\\Sandbox\\%USER%\\%SANDBOX%\\Session_%SESSION%");
		else
			theAPI->GetGlobalSettings()->SetText("IpcRootPath", ui.ipcRoot->text());


		theAPI->GetGlobalSettings()->SetBool("EditAdminOnly", ui.chkAdminOnly->isChecked());

		bool isPassSet = !theAPI->GetGlobalSettings()->GetText("EditPassword", "").isEmpty();
		if (ui.chkPassRequired->isChecked())
		{
			if (!isPassSet && m_NewPassword.isEmpty())
				OnSetPassword(); // request password entry if it wasn't entered already
			if (!m_NewPassword.isEmpty()) {
				theAPI->LockConfig(m_NewPassword); // set new/changed password
				m_NewPassword.clear();
			}
		}
		else if (isPassSet)
			theAPI->LockConfig(QString()); // clear password

		theAPI->GetGlobalSettings()->SetBool("ForceDisableAdminOnly", ui.chkAdminOnlyFP->isChecked());
		theAPI->GetGlobalSettings()->SetBool("ForgetPassword", ui.chkClearPass->isChecked());

		if (m_WarnProgsChanged)
		{
			theAPI->GetGlobalSettings()->SetBool("StartRunAlertDenied", ui.chkStartBlock->isChecked());
			theAPI->GetGlobalSettings()->SetBool("NotifyStartRunAccessDenied", ui.chkStartBlockMsg->isChecked());

			QStringList AlertProcess;
			QStringList AlertFolder;
			for (int i = 0; i < ui.treeWarnProgs->topLevelItemCount(); i++)
			{
				QTreeWidgetItem* pItem = ui.treeWarnProgs->topLevelItem(i);
				int Type = pItem->data(0, Qt::UserRole).toInt();
				switch (Type)
				{
				case 1:	AlertProcess.append(pItem->data(1, Qt::UserRole).toString()); break;
				case 2: AlertFolder.append(pItem->data(1, Qt::UserRole).toString()); break;
				}
			}

			theAPI->GetGlobalSettings()->UpdateTextList("AlertProcess", AlertProcess, false);
			theAPI->GetGlobalSettings()->UpdateTextList("AlertFolder", AlertFolder, false);
			m_WarnProgsChanged = false;
		}

		if (m_CompatChanged)
		{
			QStringList Used;
			QStringList Rejected;
			for (int i = 0; i < ui.treeCompat->topLevelItemCount(); i++) {
				QTreeWidgetItem* pItem = ui.treeCompat->topLevelItem(i);
				if (pItem->checkState(0) == Qt::Unchecked)
					Rejected.append(pItem->data(0, Qt::UserRole).toString());
				else
					Used.append(pItem->data(0, Qt::UserRole).toString());
			}

			theAPI->GetGlobalSettings()->UpdateTextList("Template", Used, false);
			theAPI->GetGlobalSettings()->UpdateTextList("TemplateReject", Rejected, false);
			m_CompatChanged = false;
		}
	}

	if (ui.chkAutoRoot->isVisible())
		theConf->SetValue("Options/PortableRootDir", ui.chkAutoRoot->checkState() == Qt::Checked ? 1 : 0);

	theConf->SetValue("Options/AutoRunSoftCompat", !ui.chkNoCompat->isChecked());

	emit OptionsChanged();
}

void CSettingsWindow::apply()
{
	SaveSettings();
	LoadSettings();
}

void CSettingsWindow::ok()
{
	SaveSettings();

	this->close();
}

void CSettingsWindow::reject()
{
	this->close();
}

void CSettingsWindow::OnBrowse()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.fileRoot->setText(Value + "\\%SANDBOX%");
}

void CSettingsWindow::OnChange()
{
	//ui.chkLinuxStyle->setEnabled(!ui.chkUseCycles->isChecked());

	QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui.onClose->model());
	QStandardItem *item = model->item(0);
	item->setFlags((!ui.chkShowTray->isChecked()) ? item->flags() & ~Qt::ItemIsEnabled : item->flags() | Qt::ItemIsEnabled);

	if (ui.chkAutoRoot->isVisible() && theGUI->IsFullyPortable())
		ui.fileRoot->setEnabled(ui.chkAutoRoot->checkState() != Qt::Checked);

	ui.btnSetPassword->setEnabled(ui.chkPassRequired->isChecked());
}

void CSettingsWindow::OnTab()
{
	if (ui.tabs->currentWidget() == ui.tabCompat && m_CompatLoaded != 1 && theAPI->IsConnected())
	{
		if(m_CompatLoaded == 0)
			theGUI->GetCompat()->RunCheck();

		ui.treeCompat->clear();

		QMap<QString, int> Templates = theGUI->GetCompat()->GetTemplates();
		for (QMap<QString, int>::iterator I = Templates.begin(); I != Templates.end(); ++I)
		{
			if (I.value() == CSbieTemplates::eNone)
				continue;

			QSharedPointer<CSbieIni> pTemplate = QSharedPointer<CSbieIni>(new CSbieIni("Template_" + I.key(), theAPI));

			QString Title = pTemplate->GetText("Tmpl.Title");
			if (Title.left(1) == "#")
			{
				int End = Title.mid(1).indexOf(",");
				if (End == -1) End = Title.length() - 1;
				int MsgNum = Title.mid(1, End).toInt();
				Title = theAPI->GetSbieMsgStr(MsgNum, theGUI->m_LanguageId).arg(Title.mid(End + 2)).arg("");
			}
			//if (Title.isEmpty()) Title = Name;

			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setText(0, Title);
			pItem->setData(0, Qt::UserRole, I.key());
			if((I.value() & CSbieTemplates::eDisabled) != 0)
				pItem->setCheckState(0, Qt::Unchecked);
			else if((I.value() & CSbieTemplates::eEnabled) != 0)
				pItem->setCheckState(0, Qt::Checked);
			else
				pItem->setCheckState(0, Qt::PartiallyChecked);
			ui.treeCompat->addTopLevelItem(pItem);
		}

		m_CompatLoaded = 1;
		m_CompatChanged = true;
	}
}

void CSettingsWindow::OnSetPassword()
{
retry:
	QString Value1 = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter the new configuration password."), QLineEdit::Password);
	if (Value1.isEmpty())
		return;

	QString Value2 = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please re-enter the new configuration password."), QLineEdit::Password);
	if (Value2.isEmpty())
		return;

	if (Value1 != Value2) {
		QMessageBox::warning(this, "Sandboxie-Plus", tr("Passwords did not match, please retry."));
		goto retry;
	}

	m_NewPassword = Value1;
}

void CSettingsWindow::AddWarnEntry(const QString& Name, int type)
{
	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, (type == 1 ? tr("Process") : tr("Folder")));
	pItem->setData(0, Qt::UserRole, type);

	pItem->setData(1, Qt::UserRole, Name);
	pItem->setText(1, Name);
	ui.treeWarnProgs->addTopLevelItem(pItem);
}

void CSettingsWindow::OnAddWarnProg()
{
	QString Value = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter a program file name"));
	if (Value.isEmpty())
		return;
	AddWarnEntry(Value, 1);
	m_WarnProgsChanged = true;
}

void CSettingsWindow::OnAddWarnFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	AddWarnEntry(Value, 2);
	m_WarnProgsChanged = true;
}

void CSettingsWindow::OnDelWarnProg()
{
	QTreeWidgetItem* pItem = ui.treeWarnProgs->currentItem();
	if (!pItem)
		return;

	delete pItem;
	m_WarnProgsChanged = true;
}

void CSettingsWindow::OnTemplateClicked(QTreeWidgetItem* pItem, int Column)
{
	// todo: check if really changed
	m_CompatChanged = true;
}

void CSettingsWindow::OnAddCompat()
{
	QTreeWidgetItem* pItem = ui.treeCompat->currentItem();
	if (!pItem)
		return;

	pItem->setCheckState(0, Qt::Checked);
	m_CompatChanged = true;
}

void CSettingsWindow::OnDelCompat()
{
	QTreeWidgetItem* pItem = ui.treeCompat->currentItem();
	if (!pItem)
		return;

	pItem->setCheckState(0, Qt::Unchecked);
	m_CompatChanged = true;
}
