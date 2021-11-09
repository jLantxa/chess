/*
 * Copyright (C) 2021  Javier Lancha Vázquez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <QColor>
#include <QInputDialog>
#include <QMessageBox>
#include <QThread>

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include "settingsdialog.h"

static const QIcon ROTATE_ICON = QIcon("res/icon/rotate.svg");
static const QIcon SETTINGS_ICON = QIcon("res/icon/settings.svg");
static const QIcon ENGINE_ICON = QIcon("res/icon/power.svg");

MainWindow::MainWindow(QWidget *parent)
:   QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_board = ui->boardWidget;
    m_settings_dialog = new SettingsDialog;
    connect(m_settings_dialog, &SettingsDialog::PaletteChanged, this, &MainWindow::SetBoardPalette);

    Init();
    connect(&m_engine, &UCIEngine::DepthInfoAvailable, this, &MainWindow::OnDepthInfoAvailable);
}

MainWindow::~MainWindow() {
    delete ui;
    m_engine.Close();
}

void MainWindow::Init() {
    // GUI defaults
    setWindowTitle(WINDOW_TITLE);
    ui->bEngineOn->setPalette(QColor(Qt::red));
    m_start_half_moves = 0;
    m_board->SetPlayingColour(chess::Colour::WHITE);

    ui->bRotateBoard->setIcon(ROTATE_ICON);
    ui->bSettings->setIcon(SETTINGS_ICON);
    ui->bEngineOn->setIcon(ENGINE_ICON);

    // Engine defaults
    m_engine.Init(DEFAULT_ENGINE_CMD);
    SetEngineEnabled(false);

    SetDepth(20);

    ui->sbDepth->setValue(m_depth);
    SetNumLines(1);

    ui->sbLines->setValue(1);

    const int max_num_threads = QThread::idealThreadCount();
    const int initial_threads = max_num_threads / 4;
    ui->sbThreads->setMaximum(max_num_threads);
    ui->sbThreads->setToolTip(QString("Number of CPU threads to use.\nMaximum number of threads: " +
                                      QString::number(max_num_threads)));
    m_engine.SetNumThreads(initial_threads);
    ui->sbThreads->setValue(initial_threads);
}

uint32_t MainWindow::CurrentMoveNumber() const {
    return 1 + ((m_start_half_moves + m_moves_list.size()) / 2);
}

void MainWindow::NewGame() {
    m_start_half_moves = 0;
    m_board->Reset();
    m_moves_list.clear();
    m_engine.NewGame();
    RestartSearch();
}

void MainWindow::SetNumLines(uint8_t num_lines) {
    m_depth_infos.clear();
    m_depth_infos.resize(num_lines);
    m_engine.SetNumLines(num_lines);
    RestartSearch();
}

void MainWindow::SetDepth(uint8_t depth) {
    m_depth = depth;
    RestartSearch();
}

void MainWindow::RestartSearch() {
    if (ui->bEngineOn->isChecked()) {
        m_engine.Stop();
        m_engine.SearchWithDepth(m_depth);
    }
}

bool MainWindow::SetPosition(const QString& fen_str){
    if (fen_str.isEmpty()) {
        return false;
    }

    const char* FEN_SEPARATOR = " ";
    const QStringList args = fen_str.split(FEN_SEPARATOR);

    if (args.length() != 6) {
        return false;
    }

    m_board->SetPosition(fen_str);

    const uint32_t move_number = args[5].toUInt();
    m_moves_list.clear();
    ui->teMoves->clear();
    m_start_half_moves = 2 * (move_number - 1);
    if (m_board->GetPlayingColour() == chess::Colour::BLACK) {
        m_start_half_moves++;
    }

    m_engine.SetPosition(fen_str);
    RestartSearch();

    return true;
}

void MainWindow::ShowMsgBox(const QString &title, const QString &text) {
    QMessageBox msg_box(this);
    msg_box.setWindowTitle(title);
    msg_box.setText(text);
    msg_box.exec();
}

void MainWindow::OnDepthInfoAvailable(const UCIEngine::DepthInfo& info) {
    const uint32_t line_id = info.line_id;
    if (line_id > m_depth_infos.size()) {
        return;
    }

    m_num_received_lines = line_id;
    m_depth_infos[line_id - 1] = info;  // Lines start counting at 1

    UpdateLineInfo();
}

void MainWindow::UpdateLineInfo() {
    ui->teLines->clear();
    for (uint32_t i = 0; i < m_num_received_lines; ++i) {
        auto& info = m_depth_infos[i];
        QStringList move_str_chain;

        /* If black plays, the first move in the sequence belongs to black, and
         * we must omit white's move:
         * n... <black> instead of n. <white> <black>
         */
        for (int i = 0; i < info.pv.length(); ++i) {
            const uint32_t half_moves = (m_start_half_moves + i);
            if ((half_moves % 2) == 0 ) {  // White move
                move_str_chain.push_back(QString::number(1 + (half_moves/2)) + ". " + info.pv[i]);
            } else {  // Black move
                if (i == 0) {
                    move_str_chain.push_back(QString::number(1 + (half_moves/2)) + "... " + info.pv[i]);
                } else {
                    move_str_chain.push_back(info.pv[i]);
                }

            }
        }

        QString score_str;
        int score = info.score;
        if (m_board->GetPlayingColour() == chess::Colour::BLACK) {
            score *= -1;
        }
        if (!info.mate_counter) {
            score_str = "<b>[" + GetSignedScore(score) + "]</b>";
        } else {
            QString signed_mate_str = (score >= 0)? "M" : "-M";
            score_str = "<b>[" + signed_mate_str + QString::number(info.score) + "]</b>";
        }

        ui->teLines->append(score_str + " " + move_str_chain.join(" ") + "<br>");

        if (i == 0) {
            m_board->SetScore(score, info.mate_counter);
        }
    }
}

void MainWindow::UpdateMoveList() {
    ui->teMoves->clear();
    const int length = m_moves_list.length();
    QString moves_str;
    for (int i = 0; i < length; ++i) {
        const QString&  move = m_moves_list[i];
        const uint32_t half_moves = (m_start_half_moves + (i+1));
        if ((half_moves % 2) != 0) {
            moves_str += QString::number(1 +( half_moves/2)) + ". " + "<b>" + move + "</b> ";
        } else {
            moves_str += "<b>" + move + "</b> ";
        }
    }

    ui->teMoves->setHtml(moves_str);
}

void MainWindow::SetEngineEnabled(bool enabled) {
    if (enabled) {
        ui->bEngineOn->setPalette(QColor(Qt::green));
        if (ui->chInfinite->isChecked()) {
            m_engine.SearchInfinite();
        } else {
            m_engine.SearchWithDepth(m_depth);
        }
    } else {
        ui->bEngineOn->setPalette(QColor(Qt::red));
        m_engine.Stop();
        ui->teLines->clear();
    }

    m_board->SetScoreEnabled(enabled);
    SetEngineControlsEnabled(enabled);
}

void MainWindow::SetEngineControlsEnabled(bool enabled) {
    ui->lDepth->setVisible(enabled);
    ui->sbDepth->setVisible(enabled);
    ui->chInfinite->setVisible(enabled);
    ui->lLines->setVisible(enabled);
    ui->sbLines->setVisible(enabled);
    ui->lThreads->setVisible(enabled);
    ui->sbThreads->setVisible(enabled);
    ui->sbDepth->setVisible(enabled);
    ui->teLines->setVisible(enabled);
}

void MainWindow::on_bEngineOn_toggled(bool checked) {
    SetEngineEnabled(checked);
}

void MainWindow::on_chInfinite_toggled(bool checked) {
    ui->lDepth->setEnabled(!checked);
    ui->sbDepth->setEnabled(!checked);

    if (!checked) {
        m_engine.Stop();
    }
}

void MainWindow::on_sbThreads_editingFinished() {
    const int threads = ui->sbThreads->value();
    m_engine.SetNumThreads(threads);
}


void MainWindow::on_sbLines_editingFinished() {
    const int num_lines = ui->sbLines->value();
    SetNumLines(num_lines);
}


void MainWindow::on_sbDepth_editingFinished() {
    const int depth = ui->sbDepth->value();
    SetDepth(depth);
}

void MainWindow::on_actionSet_FEN_position_triggered() {
    QInputDialog* dialog = new QInputDialog(this);
    dialog->setWindowTitle("Set position");
    dialog->setLabelText("FEN position:");
    dialog->resize(450, 300);
    dialog->exec();

    QString fen_str = dialog->textValue();
    const bool position_ok = SetPosition(fen_str.trimmed());
    if (!position_ok) {
        ShowMsgBox("Error", "Could not set position.");
    }
}

void MainWindow::on_actionNew_game_triggered() {
    NewGame();
}

void MainWindow::on_actionExit_triggered() {
    QCoreApplication::quit();
}

void MainWindow::on_bRotateBoard_clicked() {
    m_board->Rotate();
}


void MainWindow::on_actionSettings_triggered() {
    m_settings_dialog->exec();
}

void MainWindow::SetBoardPalette(const ChessBoardWidget::ChessPalette& palette) {
    m_board->SetColourPalette(palette);
}


void MainWindow::on_bSettings_clicked() {
    m_settings_dialog->exec();
}

