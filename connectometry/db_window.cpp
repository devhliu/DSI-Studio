#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>

#include "db_window.h"
#include "ui_db_window.h"
#include "match_db.h"
void show_view(QGraphicsScene& scene,QImage I);

db_window::db_window(QWidget *parent,std::shared_ptr<group_connectometry_analysis> vbc_) :
    QMainWindow(parent),color_bar(10,256),vbc(vbc_),
    ui(new Ui::db_window)
{
    color_map.spectrum();
    color_bar.spectrum();
    ui->setupUi(this);
    ui->report->setText(vbc->handle->db.report.c_str());
    ui->vbc_view->setScene(&vbc_scene);
    ui->fp_dif_view->setScene(&fp_dif_scene);
    ui->fp_view->setScene(&fp_scene);


    ui->subject_view->setCurrentIndex(0);

    ui->x_pos->setMaximum(vbc->handle->dim[0]-1);
    ui->y_pos->setMaximum(vbc->handle->dim[1]-1);
    ui->z_pos->setMaximum(vbc->handle->dim[2]-1);

    connect(ui->slice_pos,SIGNAL(valueChanged(int)),this,SLOT(on_subject_list_itemSelectionChanged()));

    connect(ui->view_y,SIGNAL(toggled(bool)),this,SLOT(on_view_x_toggled(bool)));
    connect(ui->view_z,SIGNAL(toggled(bool)),this,SLOT(on_view_x_toggled(bool)));

    connect(ui->zoom,SIGNAL(valueChanged(double)),this,SLOT(on_subject_list_itemSelectionChanged()));
    connect(ui->show_mask,SIGNAL(clicked()),this,SLOT(on_subject_list_itemSelectionChanged()));
    connect(ui->add,SIGNAL(clicked()),this,SLOT(on_actionAdd_DB_triggered()));

    fp_mask.resize(vbc->handle->dim);
    for(int i = 0;i < fp_mask.size();++i)
        if(vbc->handle->dir.get_fa(i,0) > 0.0f)
            fp_mask[i] = 1.0;
        else
            fp_mask[i] = 0.0;
    on_view_x_toggled(true);
    update_subject_list();
    ui->subject_list->selectRow(0);
    qApp->installEventFilter(this);


}

db_window::~db_window()
{
    delete ui;
}

void db_window::closeEvent (QCloseEvent *event)
{
    if(!vbc->handle->db.modified)
    {
        event->accept();
        return;
    }

    QMessageBox::StandardButton r = QMessageBox::question( this, "DSI Studio",
                                                                tr("Modification not saved. Save now?\n"),
                                                                QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
                                                                QMessageBox::Cancel);
    if (r == QMessageBox::Cancel)
    {
        event->ignore();
        return;
    }
    if (r == QMessageBox::No)
    {
        event->accept();
        return;
    }
    if (r == QMessageBox::Yes)
    {
        on_actionSave_DB_as_triggered();
        if(!vbc->handle->db.modified)
        {
            event->accept();
            return;
        }
        else
        {
            event->ignore();
            return;
        }
    }
}


bool db_window::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() != QEvent::MouseMove || obj->parent() != ui->vbc_view)
        return false;
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    QPointF point = ui->vbc_view->mapToScene(mouseEvent->pos().x(),mouseEvent->pos().y());
    tipl::vector<3,float> pos;
    pos[0] =  ((float)point.x()) / ui->zoom->value() - 0.5;
    pos[1] =  ((float)point.y()) / ui->zoom->value() - 0.5;
    pos[2] = ui->slice_pos->value();
    if(!vbc->handle->dim.is_valid(pos))
        return true;
    ui->x_pos->setValue(std::round(pos[0]));
    ui->y_pos->setValue(std::round(pos[1]));
    ui->z_pos->setValue(std::round(pos[2]));


    return true;
}

void db_window::update_subject_list()
{
    ui->subject_list->setCurrentCell(0,0);
    ui->subject_list->clear();
    ui->subject_list->setColumnCount(1);
    ui->subject_list->setColumnWidth(0,500);
    ui->subject_list->setRowCount(vbc->handle->db.num_subjects);
    for(unsigned int index = 0;index < vbc->handle->db.num_subjects;++index)
        ui->subject_list->setItem(index,0, new QTableWidgetItem(QString(vbc->handle->db.subject_names[index].c_str())));

}

void db_window::on_subject_list_itemSelectionChanged()
{
    if(ui->subject_list->currentRow() == -1 ||
            ui->subject_list->currentRow() >= vbc->handle->db.subject_qa.size())
        return;
    if(ui->view_x->isChecked())
        ui->x_pos->setValue(ui->slice_pos->value());
    if(ui->view_y->isChecked())
        ui->y_pos->setValue(ui->slice_pos->value());
    if(ui->view_z->isChecked())
        ui->z_pos->setValue(ui->slice_pos->value());

    tipl::image<float,2> slice;
    vbc->handle->db.get_subject_slice(ui->subject_list->currentRow(),
                                   ui->view_x->isChecked() ? 0:(ui->view_y->isChecked() ? 1:2),
                                   ui->slice_pos->value(),slice);
    tipl::normalize(slice);
    tipl::color_image color_slice(slice.geometry());
    std::copy(slice.begin(),slice.end(),color_slice.begin());
    if(ui->show_mask->isChecked())
    {
        auto mask_slice = fp_mask.slice_at(ui->slice_pos->value());
        for(int i = 0;i < color_slice.size();++i)
            if(mask_slice[i])
                color_slice[i][2] = 255;
    }
    QImage qimage((unsigned char*)&*color_slice.begin(),color_slice.width(),color_slice.height(),QImage::Format_RGB32);
    vbc_slice_image = qimage.scaled(color_slice.width()*ui->zoom->value(),color_slice.height()*ui->zoom->value());
    if(!ui->view_z->isChecked())
        vbc_slice_image = vbc_slice_image.mirrored();
    show_view(vbc_scene,vbc_slice_image);
    vbc_slice_pos = ui->slice_pos->value();

    //if(ui->subject_view->currentIndex() == 1)
    {
        std::vector<float> fp;
        float threshold = ui->fp_coverage->value()*tipl::segmentation::otsu_threshold(tipl::make_image(vbc->handle->dir.fa[0],vbc->handle->dim));
        vbc->handle->db.get_subject_vector(ui->subject_list->currentRow(),fp,fp_mask,threshold,ui->normalize_fp->isChecked());
        fp_image_buf.clear();
        fp_image_buf.resize(tipl::geometry<2>(ui->fp_zoom->value()*25,ui->fp_zoom->value()*100));// rotated

        tipl::minus_constant(fp.begin(),fp.end(),*std::min_element(fp.begin(),fp.end()));
        float max_fp = *std::max_element(fp.begin(),fp.end());
        if(max_fp == 0)
            return;
        tipl::multiply_constant(fp,(float)fp_image_buf.width()/max_fp);
        std::vector<int> ifp(fp.size());
        std::copy(fp.begin(),fp.end(),ifp.begin());
        tipl::upper_lower_threshold(ifp,0,(int)fp_image_buf.width()-1);
        unsigned int* base = (unsigned int*)&fp_image_buf[0];
        for(unsigned int i = 0;i < fp_image_buf.height();++i,base += fp_image_buf.width())
        {
            unsigned int from_index = (i)*ifp.size()/fp_image_buf.height();
            unsigned int to_index = (i+1)*ifp.size()/fp_image_buf.height();
            if(from_index < to_index)
            for(++from_index;from_index != to_index;++from_index)
            {
                unsigned int from = ifp[from_index-1];
                unsigned int to = ifp[from_index];
                if(from > to)
                    std::swap(from,to);
                tipl::add_constant(base+from,base+to,1);
            }
        }
        base = (unsigned int*)&fp_image_buf[0];
        unsigned int max_value = *std::max_element(base,base+fp_image_buf.size());
        if(max_value)
        for(unsigned int index = 0;index < fp_image_buf.size();++index)
            fp_image_buf[index] = tipl::rgb((unsigned char)(255-std::min<int>(255,(fp_image_buf[index].color*512/max_value))));
        tipl::swap_xy(fp_image_buf);
        tipl::flip_y(fp_image_buf);
        QImage fp_image_tmp((unsigned char*)&*fp_image_buf.begin(),fp_image_buf.width(),fp_image_buf.height(),QImage::Format_RGB32);
        fp_image = fp_image_tmp;
        show_view(fp_scene,fp_image);
    }

    if(!fp_dif_map.empty() && fp_dif_map.width() == vbc->handle->db.num_subjects)
    {
        fp_dif_map.resize(tipl::geometry<2>(vbc->handle->db.num_subjects,vbc->handle->db.num_subjects));
        for(unsigned int index = 0;index < fp_matrix.size();++index)
            fp_dif_map[index] = color_map[fp_matrix[index]*256.0/fp_max_value];

        // line x
        for(unsigned int x_pos = 0,pos = ui->subject_list->currentRow()*vbc->handle->db.num_subjects;x_pos < vbc->handle->db.num_subjects;++x_pos,++pos)
        {
            fp_dif_map[pos][2] = (fp_dif_map[pos][0] >> 1);
            fp_dif_map[pos][2] += 125;
        }
        // line y
        for(unsigned int y_pos = 0,pos = ui->subject_list->currentRow();y_pos < vbc->handle->db.num_subjects;++y_pos,pos += vbc->handle->db.num_subjects)
        {
            fp_dif_map[pos][2] = (fp_dif_map[pos][0] >> 1);
            fp_dif_map[pos][2] += 125;
        }
        on_fp_zoom_valueChanged(0);
    }
}

void db_window::on_actionSave_Subject_Name_as_triggered()
{
    QString filename = QFileDialog::getSaveFileName(
                this,
                "Save name list",
                windowTitle() + ".name.txt",
                "Report file (*.txt);;All files (*)");
    if(filename.isEmpty())
        return;
    std::ofstream out(filename.toLocal8Bit().begin());
    for(unsigned int index = 0;index < vbc->handle->db.num_subjects;++index)
        out << vbc->handle->db.subject_names[index] << std::endl;
}

void db_window::on_action_Save_R2_values_as_triggered()
{
    QString filename = QFileDialog::getSaveFileName(
                this,
                "Save R2 values",
                windowTitle() + ".R2.txt",
                "Report file (*.txt);;All files (*)");
    if(filename.isEmpty())
        return;
    std::ofstream out(filename.toLocal8Bit().begin());
    std::copy(vbc->handle->db.R2.begin(),vbc->handle->db.R2.end(),std::ostream_iterator<float>(out,"\n"));
}

void db_window::on_actionSave_fingerprints_triggered()
{
    QString filename = QFileDialog::getSaveFileName(
                this,
                "Save Vector",
                windowTitle() + ".vec.mat",
                "Report file (*.mat);;All files (*)");
    if(filename.isEmpty())
        return;

    float threshold = ui->fp_coverage->value()*tipl::segmentation::otsu_threshold(
                tipl::make_image(vbc->handle->dir.fa[0],vbc->handle->dim));
    vbc->handle->db.save_subject_vector(filename.toLocal8Bit().begin(),fp_mask,threshold,ui->normalize_fp->isChecked());

}

void db_window::on_actionSave_pair_wise_difference_as_triggered()
{
    QString filename = QFileDialog::getSaveFileName(
                this,
                "Save Vector",
                windowTitle() + ".vec.dif.mat",
                "MATLAB file (*.mat);;Figures (*.jpg *.png *.tif *.bmp;;All files (*)");
    if(filename.isEmpty())
        return;
    if(fp_matrix.empty() || fp_matrix.size() != vbc->handle->db.num_subjects*vbc->handle->db.num_subjects)
        on_calculate_dif_clicked();
    if(QFileInfo(filename).suffix().toLower() == "mat")
    {
        tipl::io::mat_write out(filename.toStdString().c_str());
        if(!out)
            return;
        out.write("dif",(const float*)&*fp_matrix.begin(),vbc->handle->db.num_subjects,vbc->handle->db.num_subjects);
    }
    else
    {
        QImage img(fp_dif_scene.sceneRect().size().toSize(), QImage::Format_RGB32);
        QPainter painter(&img);
        painter.fillRect(fp_dif_scene.sceneRect(),Qt::white);
        fp_dif_scene.render(&painter);
        img.save(filename);
    }
}


void db_window::on_view_x_toggled(bool checked)
{
    if(!checked)
        return;
    unsigned char dim = ui->view_x->isChecked() ? 0:(ui->view_y->isChecked() ? 1:2);
    ui->slice_pos->setMaximum(vbc->handle->dim[dim]-1);
    ui->slice_pos->setMinimum(0);
    ui->slice_pos->setValue(vbc->handle->dim[dim] >> 1);
}

void db_window::on_actionLoad_mask_triggered()
{
    QString file = QFileDialog::getOpenFileName(
                                this,
                                "Load fingerprint mask from file",
                                QFileInfo(windowTitle()).absoluteDir().absolutePath(),
                                "Report file (*.nii *nii.gz);;Text files (*.txt);;All files (*)");
    if(file.isEmpty())
        return;
    tipl::image<float,3> I;
    gz_nifti nii;
    if(!nii.load_from_file(file.toLocal8Bit().begin()))
    {
        QMessageBox::information(this,"Error","Invalid nifti file format",0);
        return;
    }
    nii.toLPS(I);
    if(I.geometry() != fp_mask.geometry())
    {
        QMessageBox::information(this,"Error","Inconsistent image dimension. Please use DSI Studio to output the mask.",0);
        return;
    }
    for(unsigned int i = 0;i < I.size();++i)
        fp_mask[i] = I[i] > 0.0f ? 1:0;

    ui->show_mask->setChecked(true);
    on_subject_list_itemSelectionChanged();

}

void db_window::on_actionSave_mask_triggered()
{
    QString FileName = QFileDialog::getSaveFileName(
                                this,
                                "Save fingerprint mask",
                                QFileInfo(windowTitle()).absoluteDir().absolutePath() + "/mask.nii.gz",
                                "Report file (*.nii *nii.gz);;Text files (*.txt);;All files (*)");
    if(FileName.isEmpty())
        return;
    float fiber_threshold = ui->fp_coverage->value()*tipl::segmentation::otsu_threshold(
                tipl::make_image(vbc->handle->dir.fa[0],vbc->handle->dim));
    tipl::image<float,3> mask(fp_mask);
    for(unsigned int index = 0;index < mask.size();++index)
        if(vbc->handle->dir.fa[0][index] < fiber_threshold)
            mask[index] = 0;
    gz_nifti::save_to_file(FileName.toStdString().c_str(),mask,vbc->handle->vs,vbc->handle->trans_to_mni,true);
}
void db_window::update_db(void)
{
    update_subject_list();
    if(!fp_dif_map.empty())
        on_calculate_dif_clicked();
    ui->report->setText(vbc->handle->db.report.c_str());

}


void db_window::on_calculate_dif_clicked()
{
    float threshold = ui->fp_coverage->value()*tipl::segmentation::otsu_threshold(
                    tipl::make_image(vbc->handle->dir.fa[0],vbc->handle->dim));
    vbc->handle->db.get_dif_matrix(fp_matrix,fp_mask,threshold,ui->normalize_fp->isChecked());
    fp_max_value = *std::max_element(fp_matrix.begin(),fp_matrix.end());
    fp_dif_map.resize(tipl::geometry<2>(vbc->handle->db.num_subjects,vbc->handle->db.num_subjects));
    for(unsigned int index = 0;index < fp_matrix.size();++index)
        fp_dif_map[index] = color_map[fp_matrix[index]*255.0/fp_max_value];
    on_fp_zoom_valueChanged(ui->fp_zoom->value());

}
QPixmap fromImage(const QImage &I);
void db_window::on_fp_zoom_valueChanged(double)
{
    QImage qimage((unsigned char*)&*fp_dif_map.begin(),fp_dif_map.width(),fp_dif_map.height(),QImage::Format_RGB32);
    fp_dif_image = qimage.scaled(fp_dif_map.width()*ui->fp_zoom->value(),fp_dif_map.height()*ui->fp_zoom->value());
    fp_dif_scene.setSceneRect(0, 0, fp_dif_image.width()+80,fp_dif_image.height()+10);
    fp_dif_scene.clear();
    fp_dif_scene.addPixmap(fromImage(fp_dif_image));

    QImage qbar((unsigned char*)&*color_bar.begin(),color_bar.width(),color_bar.height(),QImage::Format_RGB32);
    qbar = qbar.scaledToHeight(fp_dif_image.height());
    fp_dif_scene.addPixmap(fromImage(qbar))->moveBy(fp_dif_image.width()+10,0);
    fp_dif_scene.addText(QString::number(fp_max_value))->moveBy(fp_dif_image.width()+qbar.width()+10,-10);
    fp_dif_scene.addText(QString("0"))->moveBy(fp_dif_image.width()+qbar.width()+10,(int)fp_dif_image.height()-10);
}

void db_window::on_delete_subject_clicked()
{
    if(ui->subject_list->currentRow() >= 0 && vbc->handle->db.num_subjects > 1)
    {
        unsigned int index = ui->subject_list->currentRow();
        vbc->handle->db.remove_subject(index);
        if(index < ui->subject_list->rowCount())
            ui->subject_list->removeRow(index);
    }
}

void db_window::on_actionCalculate_change_triggered()
{
    float threshold = ui->fp_coverage->value()*tipl::segmentation::otsu_threshold(
    tipl::make_image(vbc->handle->dir.fa[0],vbc->handle->dim));
    vbc->handle->db.auto_match(fp_mask,threshold,ui->normalize_fp->isChecked());

    std::unique_ptr<match_db> mdb(new match_db(this,vbc));
    if(mdb->exec() == QDialog::Accepted)
        update_db();
}

void db_window::on_actionSave_DB_as_triggered()
{
    QString filename = QFileDialog::getSaveFileName(
                           this,
                           "Save Database",
                           windowTitle()+".modified.db.fib.gz",
                           "Database files (*db?fib.gz *fib.gz);;All files (*)");
    if (filename.isEmpty())
        return;
    begin_prog("saving");
    vbc->handle->db.save_subject_data(filename.toStdString().c_str());
    check_prog(0,0);
}

void db_window::on_subject_view_currentChanged(int index)
{
    if(index == 1 && (fp_matrix.empty() || fp_matrix.size() != vbc->handle->db.num_subjects*vbc->handle->db.num_subjects))
        on_calculate_dif_clicked();
}

void db_window::on_move_down_clicked()
{

    if(ui->subject_list->currentRow() >= vbc->handle->db.num_subjects-1)
        return;
    vbc->handle->db.move_down(ui->subject_list->currentRow());
    QString t = ui->subject_list->item(ui->subject_list->currentRow(),0)->text();
    ui->subject_list->item(ui->subject_list->currentRow(),0)->setText(ui->subject_list->item(ui->subject_list->currentRow()+1,0)->text());
    ui->subject_list->item(ui->subject_list->currentRow()+1,0)->setText(t);
    ui->subject_list->selectRow(ui->subject_list->currentRow()+1);
}

void db_window::on_move_up_clicked()
{
    if(ui->subject_list->currentRow() <= 0)
        return;
    vbc->handle->db.move_up(ui->subject_list->currentRow());
    QString t = ui->subject_list->item(ui->subject_list->currentRow(),0)->text();
    ui->subject_list->item(ui->subject_list->currentRow(),0)->setText(ui->subject_list->item(ui->subject_list->currentRow()-1,0)->text());
    ui->subject_list->item(ui->subject_list->currentRow()-1,0)->setText(t);
    ui->subject_list->selectRow(ui->subject_list->currentRow()-1);

}

void db_window::on_actionAdd_DB_triggered()
{
    QStringList filenames = QFileDialog::getOpenFileNames(
                           this,
                           "Open Database files",
                           windowTitle(),
                           "Database files (*db?fib.gz *fib.gz);;All files (*)");
    if (filenames.isEmpty())
        return;
    for(int i =0;i < filenames.count();++i)
    {
        std::shared_ptr<fib_data> handle(new fib_data);
        begin_prog("adding data");
        if(!handle->load_from_file(filenames[i].toStdString().c_str()))
        {
            QMessageBox::information(this,"Error",handle->error_msg.c_str(),0);
            return;
        }
        if(!handle->is_qsdr)
        {
            QMessageBox::information(this,"Error",filenames[i] + " is not from the QSDR reconstruction.",0);
            break;
        }

        if(handle->db.has_db())
        {
            if(!vbc->handle->db.add_db(handle->db))
            {
                QMessageBox::information(this,"Error",vbc->handle->error_msg.c_str(),0);
                break;
            }
            continue;
        }
        if(handle->has_odfs())
        {
            begin_prog(QFileInfo(filenames[i]).baseName().toStdString().c_str());
            if(!vbc->handle->db.add_subject_file(filenames[i].toStdString(),QFileInfo(filenames[i]).baseName().toStdString()))
            {
                QMessageBox::information(this,"error in loading subject fib files",vbc->handle->error_msg.c_str(),0);
                check_prog(0,0);
                break;
            }
            if(prog_aborted())
            {
                check_prog(0,0);
                break;
            }
        }
    }
    update_db();
}

void db_window::on_actionSelect_Subjects_triggered()
{
    QString filename = QFileDialog::getOpenFileName(
                           this,
                           "Open Selection Text files",
                           QFileInfo(windowTitle()).absoluteDir().absolutePath(),
                           "Text files (*.txt);;All files (*)");
    if (filename.isEmpty())
        return;
    std::ifstream in(filename.toStdString().c_str());
    std::vector<char> selected;
    std::copy(std::istream_iterator<int>(in),std::istream_iterator<int>(),std::back_inserter(selected));
    selected.resize(vbc->handle->db.num_subjects);
    for(int i = selected.size()-1;i >=0;--i)
        if(selected[i])
        {
            vbc->handle->db.remove_subject(i);
            ui->subject_list->removeRow(i);
        }
}
