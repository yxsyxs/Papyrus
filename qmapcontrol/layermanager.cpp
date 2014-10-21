/*
*
* This file is part of QMapControl,
* an open-source cross-platform map widget
*
* Copyright (C) 2007 - 2008 Kai Winter
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with QMapControl. If not, see <http://www.gnu.org/licenses/>.
*
* Contact e-mail: kaiwinter@gmx.de
* Program URL   : http://qmapcontrol.sourceforge.net/
*
*/

#include "layermanager.h"
namespace qmapcontrol
{
    LayerManager::LayerManager(AbstractMapControl *mapcontrol, QSize size)
            :mapcontrol(mapcontrol), scroll(QPoint(0,0)), size(size), whilenewscroll(QPoint(0,0))
    {
        // genauer berechnen?
        offSize = size *2;
        composedOffscreenImage = QPixmap(offSize);
        composedOffscreenImage2 = QPixmap(offSize);
        zoomImage = QPixmap(size);
        zoomImage.fill(Qt::white);
        screenmiddle = QPoint(size.width()/2, size.height()/2);
        useBoundingBox = false;
    }


    LayerManager::~LayerManager()
    {
        mylayers.clear();
    }

    QPointF LayerManager::currentCoordinate() const
    {
        return mapmiddle;
    }

    QPixmap LayerManager::getImage() const
    {
        return composedOffscreenImage;
    }

    Layer* LayerManager::layer() const
    {
        if ( mylayers.isEmpty() )
        {
            qDebug() << "LayerManager::getLayer() - No layers were added";
            return 0;
        }
        return mylayers.at(0) ? mylayers.at(0) : 0;
    }

    Layer* LayerManager::layer(const QString& layername) const
    {
        QListIterator<Layer*> layerit(mylayers);
        while (layerit.hasNext())
        {
            Layer* l = layerit.next();
            if (l->layerName() == layername)
                return l;
        }
        return 0;
    }

    QList<QString> LayerManager::layers() const
    {
        QList<QString> keys;
        QListIterator<Layer*> layerit(mylayers);
        while (layerit.hasNext())
        {
            keys.append(layerit.next()->layerName());
        }
        return keys;
    }


    void LayerManager::scrollView(const QPoint& point)
    {
        QPointF tempMiddle = layer()->mapAdapter()->displayToCoordinate(mapmiddle_px + point);

        if((useBoundingBox && boundingBox.contains(tempMiddle)) || !useBoundingBox)
        {
            scroll += point;
            zoomImageScroll += point;
            mapmiddle_px += point;

            mapmiddle = tempMiddle;

            if (!checkOffscreen())
            {
                newOffscreenImage();
            }
        }
    }

    void LayerManager::setView(const QPointF& coordinate)
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::setView() - cannot set view settings with no layers configured";
            return;
        }

        if ( !layer()->mapAdapter() )
        {
            qDebug() << "LayerManager::setView() - cannot set view settings with no map adapter configured";
            return;
        }

        mapmiddle_px = layer()->mapAdapter()->coordinateToDisplay(coordinate);
        mapmiddle = coordinate;

        //TODO: muss wegen moveTo() raus
        if (!checkOffscreen())
        {
            newOffscreenImage();
        }
        else
        {
            //TODO:
            // verschiebung ausrechnen
            // oder immer neues offscreenimage
            newOffscreenImage();
        }
    }

    void LayerManager::setView(QList<QPointF> coordinates)
    {
        setMiddle(coordinates);
        mapcontrol->refresh();
    }

    void LayerManager::setViewAndZoomIn(const QList<QPointF> coordinates)
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::setViewAndZoomIn() - no layers configured";
            return;
        }


        while (!containsAll(coordinates))
        {
            setMiddle(coordinates);
            zoomOut();
            // bugfix Tl
            // if points are too close -> Loop of death...
            if ( layer()->mapAdapter()->currentZoom() == 0 )
            {
                qDebug() << "LayerManager::setViewAndZoomIn() - reached minium zoom level";
                break;
            }
        }

        while (containsAll(coordinates))
        {
            setMiddle(coordinates);
            zoomIn();
            // bugfix Tl
            // if points are too close -> Loop of death...
            if ( layer()->mapAdapter()->currentZoom() == 17 )
            {
                qDebug() << "LayerManager::setViewAndZoomIn() - reached maximum zoom level";
                break;
            }
        }

        if (!containsAll(coordinates))
        {
            zoomOut();
        }

        mapcontrol->refresh();
    }

    void LayerManager::setMiddle(QList<QPointF> coordinates)
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::setMiddle() - no layers configured";
            return;
        }
        
        int sum_x = 0;
        int sum_y = 0;
        for (int i=0; i<coordinates.size(); i++)
        {
            // mitte muss in px umgerechnet werden, da aufgrund der projektion die mittebestimmung aus koordinaten ungenau ist
            QPoint p = layer()->mapAdapter()->coordinateToDisplay(coordinates.at(i));
            sum_x += p.x();
            sum_y += p.y();
        }
        QPointF middle = layer()->mapAdapter()->displayToCoordinate(QPoint(sum_x/coordinates.size(), sum_y/coordinates.size()));
        // middle in px rechnen!

        setView(middle);
    }

    bool LayerManager::containsAll(QList<QPointF> coordinates) const
    {
        QRectF bb = getViewport();
        bool containsall = true;
        for (int i=0; i<coordinates.size(); i++)
        {
            if (!bb.contains(coordinates.at(i)))
                return false;
        }
        return containsall;
    }

    QPoint LayerManager::getMapmiddle_px() const
    {
        return mapmiddle_px;
    }

    QRectF LayerManager::getViewport() const
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::getViewport() - no layers configured";
            return QRectF();
        }

        QPoint upperLeft = QPoint(mapmiddle_px.x()-screenmiddle.x(), mapmiddle_px.y()+screenmiddle.y());
        QPoint lowerRight = QPoint(mapmiddle_px.x()+screenmiddle.x(), mapmiddle_px.y()-screenmiddle.y());

        QPointF ulCoord = layer()->mapAdapter()->displayToCoordinate(upperLeft);
        QPointF lrCoord = layer()->mapAdapter()->displayToCoordinate(lowerRight);

        QRectF coordinateBB = QRectF(ulCoord, QSizeF( (lrCoord-ulCoord).x(), (lrCoord-ulCoord).y()));
        return coordinateBB;
    }

    void LayerManager::addLayer(Layer* layer)
    {
        mylayers.append(layer);

        layer->setSize(size);

        //sanity check first
        disconnect( layer, 0, this, 0 );

        connect(layer, SIGNAL(updateRequest(QRectF)),
                this, SLOT(updateRequest(QRectF)));
        connect(layer, SIGNAL(updateRequest()),
                this, SLOT(updateRequest()));

        if (mylayers.size() > 0)
        {
            //setView(QPointF(0,0));
        }
        mapcontrol->refresh();
    }

    void LayerManager::removeLayer(Layer* layer)
    {
        if ( layer )
        {
            disconnect( layer, 0, this, 0 );
            mylayers.removeAll(layer);
        }

        if (mylayers.size() > 0)
        {
            //setView(QPointF(0,0));
        }
        mapcontrol->refresh();
    }

    void LayerManager::newOffscreenImage(bool clearImage, bool showZoomImage)
    {
        // 	qDebug() << "LayerManager::newOffscreenImage()";
        whilenewscroll = mapmiddle_px;

        if (clearImage)
        {
            composedOffscreenImage2.fill(Qt::white);
        }

        QPainter painter(&composedOffscreenImage2);
        if (showZoomImage)
        {
            painter.drawPixmap(screenmiddle.x()-zoomImageScroll.x(), screenmiddle.y()-zoomImageScroll.y(),zoomImage);
        }
        //only draw basemaps
        for (int i=0; i<mylayers.count(); i++)
        {
            Layer* l = mylayers.at(i);
            if (l->isVisible())
            {
                if (l->layerType() == Layer::MapLayer)
                {
                    l->drawYourImage(&painter, whilenewscroll);
                }
            }
        }

        composedOffscreenImage = composedOffscreenImage2;
        scroll = mapmiddle_px-whilenewscroll;

        mapcontrol->refresh();
    }

    void LayerManager::zoomIn()
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::zoomIn() - no layers configured";
            return;
        }

        ImageManager::instance()->abortLoading();
        //QCoreApplication::processEvents();

        // layer rendern abbrechen?
        zoomImageScroll = QPoint(0,0);

        zoomImage.fill(Qt::white);
        QPixmap tmpImg = composedOffscreenImage.copy(screenmiddle.x()+scroll.x(),screenmiddle.y()+scroll.y(), size.width(), size.height());

        QPainter painter(&zoomImage);
        painter.translate(screenmiddle);
        painter.scale(2, 2);
        painter.translate(-screenmiddle);

        painter.drawPixmap(0,0,tmpImg);

        QListIterator<Layer*> it(mylayers);
        //TODO: remove hack, that mapadapters wont get set zoom multiple times
        QList<const MapAdapter*> doneadapters;
        while (it.hasNext())
        {
            Layer* l = it.next();
            if (!doneadapters.contains(l->mapAdapter()))
            {
                l->zoomIn();
                doneadapters.append(l->mapAdapter());
            }
        }
        mapmiddle_px = layer()->mapAdapter()->coordinateToDisplay(mapmiddle);
        whilenewscroll = mapmiddle_px;

        newOffscreenImage();
    }

    bool LayerManager::checkOffscreen() const
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::checkOffscreen() - no layers configured";
            return true;
        }

        // calculate offscreenImage dimension (px)
        QPoint upperLeft = mapmiddle_px - screenmiddle;
        QPoint lowerRight = mapmiddle_px + screenmiddle;
        QRect viewport = QRect(upperLeft, lowerRight);

        QRect testRect = layer()->offscreenViewport();

        if (!testRect.contains(viewport))
        {
            return false;
        }

        return true;
    }
    void LayerManager::zoomOut()
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::zoomOut() - no layers configured";
            return;
        }

        //QCoreApplication::processEvents();
        ImageManager::instance()->abortLoading();
        zoomImageScroll = QPoint(0,0);
        zoomImage.fill(Qt::white);
        QPixmap tmpImg = composedOffscreenImage.copy(screenmiddle.x()+scroll.x(),screenmiddle.y()+scroll.y(), size.width(), size.height());
        QPainter painter(&zoomImage);
        painter.translate(screenmiddle);
        painter.scale(0.500001,0.500001);
        painter.translate(-screenmiddle);
        painter.drawPixmap(0,0,tmpImg);

        painter.translate(screenmiddle);
        painter.scale(2,2);
        painter.translate(-screenmiddle);

        QListIterator<Layer*> it(mylayers);
        //TODO: remove hack, that mapadapters wont get set zoom multiple times
        QList<const MapAdapter*> doneadapters;
        while (it.hasNext())
        {
            Layer* l = it.next();
            if (!doneadapters.contains(l->mapAdapter()))
            {
                l->zoomOut();
                doneadapters.append(l->mapAdapter());
            }
        }
        mapmiddle_px = layer()->mapAdapter()->coordinateToDisplay(mapmiddle);
        whilenewscroll = mapmiddle_px;
        newOffscreenImage();
    }

    void LayerManager::setZoom(int zoomlevel)
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::setZoom() - no layers configured";
            return;
        }

        int current_zoom;
        if (layer()->mapAdapter()->minZoom() < layer()->mapAdapter()->maxZoom())
        {
            current_zoom = layer()->mapAdapter()->currentZoom();
        }
        else
        {
            current_zoom = layer()->mapAdapter()->minZoom() - layer()->mapAdapter()->currentZoom();
        }


        if (zoomlevel < current_zoom)
        {
            for (int i=current_zoom; i>zoomlevel; i--)
            {
                zoomOut();
            }
        }
        else
        {
            for (int i=current_zoom; i<zoomlevel; i++)
            {
                zoomIn();
            }
        }
    }

    void LayerManager::mouseEvent(const QMouseEvent* evnt)
    {
        if ( mapcontrol && !mapcontrol->mouseWheelEventsEnabled() )
        {
            return;
        }
        
        foreach( Layer* l, mylayers )
        {
            if (l && l->isVisible() )
            {
                l->mouseEvent(evnt, mapmiddle_px);
            }
        }
    }

    void LayerManager::updateRequest(QRectF rect)
    {
        if (getViewport().contains(rect.topLeft()) || getViewport().contains(rect.bottomRight()))
        {
            // QPoint point = getLayer()->getMapAdapter()->coordinateToDisplay(c);
            // const QPoint topleft = mapmiddle_px - screenmiddle;
            // QPoint finalpoint = point-topleft;
            // QRect rect_px = QRect(int(finalpoint.x()-(rect.width()-1)/2), int(finalpoint.y()-(rect.height()-1)/2),
            //  int(rect.width()+1), int(rect.height()+1));
            //
            // mapcontrol->updateRequest(rect_px);
            mapcontrol->refresh();
            //newOffscreenImage();
        }
    }
    void LayerManager::updateRequest()
    {
        newOffscreenImage();
    }
    void LayerManager::forceRedraw()
    {
        newOffscreenImage();
    }
    void LayerManager::removeZoomImage()
    {
        zoomImage.fill(Qt::white);
        forceRedraw();
    }

    void LayerManager::drawGeoms(QPainter* painter)
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::drawGeoms() - no layers configured";
            return;
        }
        QListIterator<Layer*> it(mylayers);
        while (it.hasNext())
        {
            Layer* l = it.next();
            if (l->layerType() == Layer::GeometryLayer && l->isVisible())
            {
                l->drawYourGeometries(painter, mapmiddle_px, layer()->offscreenViewport());
            }
        }
    }
    void LayerManager::drawImage(QPainter* painter)
    {
        painter->drawPixmap(-scroll.x()-screenmiddle.x(),
                            -scroll.y()-screenmiddle.y(),
                            composedOffscreenImage);
    }

    int LayerManager::currentZoom() const
    {
        if ( !layer() )
        {
            qDebug() << "LayerManager::currentZoom() - no layers configured";
            return 0;
        }
        return layer()->mapAdapter()->currentZoom();
    }

    void LayerManager::resize(QSize newSize)
    {
        size = newSize;
        offSize = newSize *2;
        composedOffscreenImage = QPixmap(offSize);
        composedOffscreenImage2 = QPixmap(offSize);
        zoomImage = QPixmap(newSize);
        zoomImage.fill(Qt::white);

        screenmiddle = QPoint(newSize.width()/2, newSize.height()/2);

        QListIterator<Layer*> it(mylayers);
        while (it.hasNext())
        {
            Layer* l = it.next();
            l->setSize(newSize);
        }

        newOffscreenImage();
        forceRedraw();
    }

    void LayerManager::setUseBoundingBox( bool usebounds )
    {
        useBoundingBox = usebounds;
    }

    bool LayerManager::isBoundingBoxEnabled()
    {
        return useBoundingBox;
    }
    
    void LayerManager::setBoundingBox( QRectF &rect )
    {
        if( rect.right() < rect.left() )
            qDebug() << "LayerManager::setBoundingBox() - min longitude is bigger than max";

        if( rect.top() < rect.bottom() )
            qDebug() << "LayerManager::setBoundingBox() - min latitude is bigger than max";

        boundingBox = rect;
    }

    QRectF LayerManager::getBoundingBox()
    {
        return boundingBox;
    }
}
