/***************************************************************************
 *   Copyright 2007 Robert Gruber <rgruber@users.sourceforge.net>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "checkoutdialog.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <interfaces/icore.h>
#include <interfaces/iruncontroller.h>

#include "cvsplugin.h"
#include "cvsjob.h"
#include "cvsproxy.h"
#include "debug.h"

CheckoutDialog::CheckoutDialog(CvsPlugin* plugin, QWidget *parent)
    : QDialog(parent), Ui::CheckoutDialogBase(), m_plugin(plugin)
{
    Ui::CheckoutDialogBase::setupUi(this);

    localWorkingDir->setMode(KFile::Directory);
}

CheckoutDialog::~CheckoutDialog()
{
}

void CheckoutDialog::accept()
{
    CvsJob *job = m_plugin->proxy()->checkout(
                    localWorkingDir->url(),
                    serverPath->text(),
                    module->currentText(),
                    QString(),
                    tag->text());
    if (job) {
        connect(job, &CvsJob::result,
                this, &CheckoutDialog::jobFinished);
        KDevelop::ICore::self()->runController()->registerJob(job);
    }
}

void CheckoutDialog::jobFinished(KJob * job)
{
    if (job->error()) {
        KMessageBox::error(this, i18n("Error on checkout"), i18n("Checkout Error"));
        return;
    }

    // The job finished, now let's check the output is everything was OK
    CvsJob* cvsjob = static_cast<CvsJob*>(job);

    static QRegExp re_file(QStringLiteral("^.\\s(.*)"));
    bool error = false;
    const QStringList lines = cvsjob->output().split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.isEmpty()) {
            // ignore empty lines
            continue;
        } else if (re_file.exactMatch(line)) {
            // line that tell us that a file has been checkedout
            continue;
        } else {
            // any other line must mean that an error occurred
            qCDebug(PLUGIN_CVS) << line;
            error = true;
        }
    }

    if (error) {
        KMessageBox::error(this,
            i18n("Some errors occurred while checking out into %1", localWorkingDir->url().toLocalFile()),
            i18n("Checkout Error"));
    } else {
        QDialog::accept();
    }
}

