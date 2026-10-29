#ifndef SLIDER
#define SLIDER


#include <QSlider>


class Slider: public QSlider

{
    Q_OBJECT

    private:



    public:
     explicit Slider( QWidget *parent = 0);
       ~Slider();





    protected:
     void mousePressEvent(QMouseEvent *event) override;


    private slots:


};





#endif // SLIDER

