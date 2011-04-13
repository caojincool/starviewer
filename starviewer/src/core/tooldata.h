#ifndef UDGTOOLDATA_H
#define UDGTOOLDATA_H

#include <QObject>

namespace udg {

/**
    Classe genèrica per a les dades d'un tool

    @author Grup de Gràfics de Girona  ( GGG ) <vismed@ima.udg.es>
*/
class ToolData : public QObject {
Q_OBJECT
public:
    ToolData(QObject *parent = 0);
    ~ToolData();

signals:
    /// Senyal que indica que les dades han canviat
    void changed();
};

}

#endif
