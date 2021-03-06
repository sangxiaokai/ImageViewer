#include "imageviewer.h"
#include "ui_imageviewer.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QMessageBox>
#include "imageview.h"

ImageViewer::ImageViewer(QWidget *parent) : QMainWindow(parent), ui(new Ui::ImageViewer){
	gridLayout = new ImageGridLayout;
	ui->setupUi(this);
	contentDownloader = 0;
	connect(ui->pushButton, SIGNAL(clicked()), this, SLOT(fetchImages()));
	ui->scrollAreaWidgetContents->setLayout(gridLayout);
}

ImageViewer::~ImageViewer() {
	delete ui;
}


void ImageViewer::fetchImages() {
	if(contentDownloader)
		contentDownloader->deleteLater();

	if(!ui->lineEdit->text().startsWith("http://") && !ui->lineEdit->text().startsWith("https://"))
		ui->lineEdit->setText("http://" + ui->lineEdit->text());

	contentDownloader = accessManager.get(QNetworkRequest(QUrl(ui->lineEdit->text())));
	if(contentDownloader)
		connect(contentDownloader, SIGNAL(finished()), this, SLOT(finishedFetching()));
	else
		QMessageBox::warning(this, "Error", "Malformed URL?");
}

void ImageViewer::finishedFetching() {
	foreach(QObject* qO, ui->scrollAreaWidgetContents->children()) {
		ImageView* view = qobject_cast<ImageView*>(qO);
		if(!view)
			continue;
		view->deleteLater();
	}

	if(contentDownloader->error()){
		QMessageBox::warning(this, "Error", "Unable to recieve data from URL.");
		contentDownloader->deleteLater();
		contentDownloader = 0;
		return;
	}

	loadedUrls.clear();
	QString line;
	while(!contentDownloader->atEnd()){
		line = contentDownloader->readLine();
		QRegExp tag("<img"); //<img[^>]*
		tag.setCaseSensitivity(Qt::CaseInsensitive);
		QRegExp extension("(http|https)://");
		extension.setCaseSensitivity(Qt::CaseInsensitive);
		int pos = line.indexOf(tag), pos2 = line.indexOf(">", pos);
		while(pos > -1 && pos2 > pos) {
			bool foundSrc = false;
			QStringList parts = line.mid(pos, pos2 - pos).split(QRegExp("(=|\\s)"), QString::SkipEmptyParts);
			for(int i = 0; i < parts.length(); i++){
				QString part = parts[i];
				if(foundSrc) {
					if(part.startsWith('"') || part.startsWith('\''))
						part = part.mid(1, part.length()-2);
					QUrl imageURL;
					if(part.startsWith("//")) {
						imageURL = "http://" + part.remove(0, 2);
					} else if(part.startsWith('/')) {
						imageURL = contentDownloader->url();
						imageURL.setPath(part);
					} else if(!extension.exactMatch(part)) {
						imageURL = contentDownloader->url();
						QString cpath = imageURL.path();
						if(!cpath.endsWith('/'))
							cpath += '/';
						imageURL.setPath(cpath + part);
					} else {
						imageURL.setUrl(part);
					}
					if(loadedUrls.contains(imageURL.toString()))
						break;
					loadedUrls.append(imageURL.toString());
					QNetworkReply* req = accessManager.get(QNetworkRequest(imageURL));
					if(req) {
						gridLayout->addWidget(new ImageView(req, ui->scrollAreaWidgetContents));
						//ui->scrollArea->updateGeometry();
						ui->scrollAreaWidgetContents->updateGeometry();
						qDebug() << "Found image URL:" << imageURL;
					}
					break;
				}
				if(part.toLower() == "src")
					foundSrc = true;
			}
			pos = line.indexOf(tag, pos2), pos2 = line.indexOf(">", pos);
		}
	}
	if(loadedUrls.length() == 0) {
		QMessageBox::warning(this, "Warning", "No image sources found on the provided URL.");
	}
	contentDownloader->deleteLater();
	contentDownloader = 0;
}
