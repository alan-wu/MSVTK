/*
 *  msvQVTKButtons.cpp
 *  VTKButtons
 *
 *  Created by Roberto Mucci on 13/01/12.
 *  Copyright 2012 B3C. All rights reserved.
 *
 *  See License at: http://tiny.cc/QXJ4D
 *
 */

#include "msvQVTKButtons.h"
#include <QImage>
#include <QDir>

#include "msvQVTKAnimate.h"

#include <vtkSmartPointer.h>
#include <vtkAlgorithmOutput.h>
#include <vtkQImageToImageSource.h>

#include <vtkTextProperty.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>

#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRendererCollection.h>
#include <vtkButtonWidget.h>
#include <vtkTexturedButtonRepresentation.h>
#include <vtkTexturedButtonRepresentation2D.h>
#include <vtkBalloonRepresentation.h>
#include <vtkCommand.h>


#include <vtkEllipticalButtonSource.h>
#include <vtkTexturedButtonRepresentation.h>

#include <vtkRenderWindow.h>
#include <vtkWindowToImageFilter.h>
#include <vtkDataSetMapper.h>
#include <vtkDataSet.h>
#include <vtkCamera.h>
#include <vtkPointData.h>

#define VTK_CREATE(type, name) vtkSmartPointer<type> name = vtkSmartPointer<type>::New()

// Callback respondign to vtkCommand::StateChangedEvent
class vtkButtonCallback : public vtkCommand {
public:
    static vtkButtonCallback *New() { 
        return new vtkButtonCallback; 
    }

    virtual void Execute(vtkObject *caller, unsigned long, void*) {
        Q_UNUSED(caller);
        msvQVTKAnimate *animateCamera = new msvQVTKAnimate();
        if (flyTo) {
            animateCamera->flyTo(renderer, bounds, 200);
        } else {
            renderer->ResetCamera(bounds);
        }
        if(animateCamera) {
            delete animateCamera;
        }
        //selection
    }

    void setBounds(double b[6]) {
        bounds[0] = b[0]; 
        bounds[1] = b[1];
        bounds[2] = b[2];
        bounds[3] = b[3];
        bounds[4] = b[4];
        bounds[5] = b[5];
    }

    void setFlyTo(bool fly) {
        flyTo = fly;
    }

    vtkButtonCallback():toolButton(NULL), renderer(0), flyTo(true) {}
    msvQVTKButtons *toolButton;
    vtkRenderer *renderer;
    double bounds[6];
    bool flyTo;
};

// Callback respondign to vtkCommand::HighlightEvent
class MSV_QT_WIDGETS_EXPORT vtkButtonHighLightCallback : public vtkCommand {
public:
    static vtkButtonHighLightCallback *New() { 
        return new vtkButtonHighLightCallback; 
    }

    virtual void Execute(vtkObject *caller, unsigned long, void*) {
        vtkTexturedButtonRepresentation2D *rep = reinterpret_cast<vtkTexturedButtonRepresentation2D*>(caller);
        int highlightState = rep->GetHighlightState();
       
        if ( highlightState == vtkButtonRepresentation::HighlightHovering && previousHighlightState == vtkButtonRepresentation::HighlightNormal ) {
            //show tooltip (not if previous state was selecting
            toolButton->setShowTooltip(true);        
        } else if ( highlightState == vtkButtonRepresentation::HighlightNormal) {
            //hide tooltip
            toolButton->setShowTooltip(false);
        } 
        previousHighlightState = highlightState;
    }

    vtkButtonHighLightCallback():toolButton(NULL), previousHighlightState(0) {}
    msvQVTKButtons *toolButton;
    int previousHighlightState;
        
};

msvQVTKButtons::msvQVTKButtons(QObject *parent) : QObject(parent), m_ShowButton(false), m_ShowLabel(true), m_FlyTo(true), m_OnCenter(false), m_ButtonWidget(NULL), m_Window(NULL) {
    vtkTexturedButtonRepresentation2D* rep = vtkTexturedButtonRepresentation2D::New();
    rep->SetNumberOfStates(1);
    
    buttonCallback = vtkButtonCallback::New();
    buttonCallback->toolButton = this;
    
    highlightCallback = vtkButtonHighLightCallback::New();
    highlightCallback->toolButton = this;

    m_ButtonWidget = NULL;
    button()->SetRepresentation(rep);
    button()->AddObserver(vtkCommand::StateChangedEvent,buttonCallback);
    rep->AddObserver(vtkCommand::HighlightEvent,highlightCallback);

}

msvQVTKButtons::~msvQVTKButtons() {
    button()->Delete();
    if(m_Window) {
        m_Window->Delete();
    }
}

vtkButtonWidget *msvQVTKButtons::button() {
   if(m_ButtonWidget == NULL) {
       m_ButtonWidget = vtkButtonWidget::New();
   }
   return m_ButtonWidget;
} 


void msvQVTKButtons::setCurrentRenderer(vtkRenderer *renderer) {
    if(renderer) {
        button()->SetInteractor(renderer->GetRenderWindow()->GetInteractor());
        button()->SetCurrentRenderer(renderer); //to check
        buttonCallback->renderer = renderer;
        button()->EnabledOn();
    } else {
        button()->SetInteractor(NULL);
        button()->SetCurrentRenderer(NULL); //to check
        buttonCallback->renderer = NULL;
        button()->EnabledOff();
    }
}

void msvQVTKButtons::setIconFileName(QString iconFileName) {
    m_IconFileName = iconFileName;
    QImage image;
    image.load(m_IconFileName);
    VTK_CREATE(vtkQImageToImageSource, imageToVTK);
    imageToVTK->SetQImage(&image);
    imageToVTK->Update();
    vtkTexturedButtonRepresentation2D *rep = static_cast<vtkTexturedButtonRepresentation2D *>(button()->GetRepresentation());
    rep->SetButtonTexture(0, imageToVTK->GetOutput());
    
    int size[2]; size[0] = 16; size[1] = 16;
    rep->GetBalloon()->SetImageSize(size);
    
    update();
}

void msvQVTKButtons::setBounds(double b[6]) {
    buttonCallback->setBounds(b);
    int i = 0;
    for( ; i<6 ; i++ ) {
        m_Bounds[i] = b[i];
    }

    update();
}

void msvQVTKButtons::calculatePosition() {
    //modify position of the vtkButton 
    double bds[3];
    if (m_OnCenter) {
        bds[0] = (m_Bounds[1] + m_Bounds[0])*.5;
        bds[1] = (m_Bounds[3] + m_Bounds[2])*.5;
        bds[2] = (m_Bounds[5] + m_Bounds[4])*.5;
    } else {
        //on the corner of the bounding box of the VME.
        bds[0] = m_Bounds[0];
        bds[1] = m_Bounds[2]; 
        bds[2] = m_Bounds[4];
    }
    int size[2]; size[0] = 16; size[1] = 16;
    vtkTexturedButtonRepresentation2D *rep = static_cast<vtkTexturedButtonRepresentation2D *>(button()->GetRepresentation());
    
    rep->PlaceWidget(bds, size);
    rep->Modified();
    button()->SetRepresentation(rep);
}

void msvQVTKButtons::update() {
    calculatePosition();
    vtkTexturedButtonRepresentation2D *rep = reinterpret_cast<vtkTexturedButtonRepresentation2D*>(button()->GetRepresentation());

    if (m_ShowLabel) {
        //Add a label to the button and change its text property
        rep->GetBalloon()->SetBalloonText(m_Label.toAscii());
        vtkTextProperty *textProp = rep->GetBalloon()->GetTextProperty();
        rep->GetBalloon()->SetPadding(2);
        textProp->SetFontSize(13);
        textProp->BoldOff();
        //textProp->SetColor(0.9,0.9,0.9);

        //Set label position
        rep->GetBalloon()->SetBalloonLayoutToImageLeft();

        //This method allows to set the label's background opacity
        rep->GetBalloon()->GetFrameProperty()->SetOpacity(0.65);
    } else {
      rep->GetBalloon()->SetBalloonText("");
    }
    
    if(m_ShowButton) {
        button()->GetRepresentation()->SetVisibility(true);
        button()->EnabledOn();
    } else {
        button()->GetRepresentation()->SetVisibility(false);
        button()->EnabledOff();
    }
    
    if(buttonCallback) {
        buttonCallback->flyTo = m_FlyTo;
        
        if(buttonCallback->renderer) {
            buttonCallback->renderer->GetRenderWindow()->Render();
        }
    }
    
}

void msvQVTKButtons::setFlyTo(bool active) {
    m_FlyTo = active;
    update();
}

void msvQVTKButtons::setToolTip(QString text) {
    m_Tooltip = text;
}

QImage msvQVTKButtons::getPreview(int width, int height) {
  if(m_Data) {
    double bounds[6];
    m_Data->GetBounds(bounds);

    // create pipe
    vtkWindowToImageFilter *previewer = vtkWindowToImageFilter::New();
    vtkDataSetMapper *mapper = vtkDataSetMapper::New();
    vtkActor *actor = vtkActor::New();
    vtkRenderer *renderer = vtkRenderer::New();
    if(m_Window == NULL) {
        m_Window = vtkRenderWindow::New();
        m_Window->SetDisplayId(NULL);
    }

    mapper->SetInput(m_Data);
    mapper->Update();
    actor->SetMapper(mapper);

    // offscreen rendering
    m_Window->OffScreenRenderingOn();
    m_Window->AddRenderer(renderer);
    m_Window->Start();
    m_Window->SetSize(width,height);
    renderer->AddActor(actor);
    renderer->ResetCamera(bounds);
    // Extract the image from the 'hidden' renderer
    previewer->SetInput(m_Window);
    previewer->Modified();
    previewer->Update();

    // Convert image data to QImage and return
    vtkImageData* vtkImage = previewer->GetOutput();
    if(!vtkImage)
        return QImage();
    vtkUnsignedCharArray* scalars = vtkUnsignedCharArray::SafeDownCast(vtkImage->GetPointData()->GetScalars());
    if(!width || !height || !scalars)
        return QImage();
    QImage qImage(width, height, QImage::Format_ARGB32);
    vtkIdType tupleIndex=0;
    int qImageBitIndex=0;
    QRgb* qImageBits = (QRgb*)qImage.bits();
    unsigned char* scalarTuples = scalars->GetPointer(0);
    for(int j=0; j<height; j++) {
        for(int i=0; i<width; i++) {
            unsigned char* tuple = scalarTuples+(tupleIndex++*3);
            QRgb color = qRgba(tuple[0], tuple[1], tuple[2], 255);
            *(qImageBits+(qImageBitIndex++))=color;
        }
    }

    previewer->SetInput(NULL);
    previewer->Delete();

    mapper->SetInput(NULL);
    mapper->Delete();

    actor->SetMapper(NULL);
    actor->Delete();

    m_Window->RemoveRenderer(renderer);
    //destroy pipe
    renderer->Delete();

    return qImage;
  }
  return QImage();
}

void msvQVTKButtons::setData(vtkDataSet *data) {
    m_Data = data;
}
