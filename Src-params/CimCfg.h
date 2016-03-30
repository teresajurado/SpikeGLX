#ifndef CIMCFG_H
#define CIMCFG_H

#include "SGLTypes.h"

#include <QMap>
#include <QString>
#include <QVector>

class QSettings;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

struct IMRODesc
{
    qint16  bank,
            refid,  // reference index
            apgn,   // gain, not index
            lfgn;   // gainm not index

    IMRODesc()
    : bank(0), refid(0), apgn(50), lfgn(50) {}
    IMRODesc( qint16 bank, qint16 refid, qint16 apgn, qint16 lfgn )
    : bank(bank), refid(refid), apgn(apgn), lfgn(lfgn) {}
    bool operator==( const IMRODesc &rhs ) const
        {return bank==rhs.bank && refid==rhs.refid
            &&  apgn==rhs.apgn &&  lfgn==rhs.lfgn;}
    QString toString( int chn ) const;
    static IMRODesc fromString( const QString &s );
};


struct IMROTbl
{
    enum imLims {
        imOpt1Elec  = 384,
        imOpt2Elec  = 384,
        imOpt3Elec  = 960,
        imOpt4Elec  = 966,

        imOpt1Banks = 1,
        imOpt2Banks = 1,
        imOpt3Banks = 3,
        imOpt4Banks = 4,

        imOpt1Chan  = 384,
        imOpt2Chan  = 384,
        imOpt3Chan  = 384,
        imOpt4Chan  = 276,

        imOpt1Refs  = 11,
        imOpt2Refs  = 11,
        imOpt3Refs  = 11,
        imOpt4Refs  = 8,

        imNGains    = 8
    };

    quint32             pSN,
                        opt;
    QVector<IMRODesc>   e;

    void fillDefault( quint32 pSN, int opt );

    bool operator==( const IMROTbl &rhs ) const
        {return opt==rhs.opt && e == rhs.e;}
    bool operator!=( const IMROTbl &rhs ) const
        {return !(*this == rhs);}

    QString toString() const;
    void fromString( const QString &s );

    bool loadFile( QString &msg, const QString &path );
    bool saveFile( QString &msg, const QString &path );

    static int elToCh384( int el );
    static int elToCh276( int el );
    static int chToEl384( int ch, int bank );
    static int chToEl276( int ch, int bank );
    static int chToRefid384( int ch );
    static int chToRefid276( int ch );
    static int elToRefid384( int el );
    static int elToRefid276( int el );
    static int idxToGain( int idx );
    static int gainToIdx( int gain );
};


// Base class for IMEC configuration
//
class CimCfg
{
    // ---------
    // Constants
    // ---------

public:
    enum imTypeId {
        imTypeAP    = 0,
        imTypeLF    = 1,
        imTypeSY    = 2,
        imSumAP     = 0,
        imSumNeural = 1,
        imSumAll    = 2,
        imNTypes    = 3
    };

    // -----
    // Types
    // -----

    struct IMVers {
        QString hwr,
                bas,
                api,
                pSN;
        int     opt;    // [1,4]
        void clear() {hwr="",bas="",api="",pSN="",opt=0;}
    };

    // ------
    // Params
    // ------

    // derived:
    // imCumTypCnt[]

public:
    VRange      range;
    double      srate;
    QString     imroFile;
    IMROTbl     roTbl;
    int         imCumTypCnt[imNTypes];
    int         hpFltIdx;
    bool        enabled,
                doGainCor,
                softStart;

    CimCfg() : range(VRange(-0.6,0.6)), srate(3e4) {}

    // -------------
    // Param methods
    // -------------

public:
    double chanGain( int ic ) const;

    void deriveChanCounts( int opt );

    void loadSettings( QSettings &S );
    void saveSettings( QSettings &S ) const;

    static int idxToFlt( int idx );

    // ------
    // Config
    // ------

    static bool getVersions( QStringList &sl, IMVers &imVers );
};

#endif  // CIMCFG_H


