#ifndef ATHBTFILTER_H_
#define ATHBTFILTER_H_


typedef enum _ATHBT_HCI_CTRL_TYPE {
    ATHBT_HCI_COMMAND     = 0,
    ATHBT_HCI_EVENT       = 1,
} ATHBT_HCI_CTRL_TYPE;

typedef enum _ATHBT_STATE_INDICATION {
    ATH_BT_NOOP        = 0,
    ATH_BT_INQUIRY     = 1,
    ATH_BT_CONNECT     = 2,
    ATH_BT_SCO         = 3,
    ATH_BT_ACL         = 4,
    ATH_BT_A2DP        = 5,
    ATH_BT_ESCO        = 6,
    /* new states go here.. */

    ATH_BT_MAX_STATE_INDICATION
} ATHBT_STATE_INDICATION;

    /* filter function for OUTGOING commands and INCOMMING events */
typedef void   (*ATHBT_FILTER_CMD_EVENTS_FN)(void *pContext, ATHBT_HCI_CTRL_TYPE Type, unsigned char *pBuffer, int Length);

    /* filter function for OUTGOING data HCI packets */
typedef void   (*ATHBT_FILTER_DATA_FN)(void *pContext, unsigned char *pBuffer, int Length);

typedef enum _ATHBT_STATE {
    STATE_OFF  = 0,
    STATE_ON   = 1,
    STATE_MAX
} ATHBT_STATE;

    /* BT state indication (when filter functions are not used) */

typedef void   (*ATHBT_INDICATE_STATE_FN)(void *pContext, ATHBT_STATE_INDICATION Indication, ATHBT_STATE State, unsigned char LMPVersion);

typedef struct _ATHBT_FILTER_INSTANCE {
#ifdef UNDER_CE
    WCHAR                       *pWlanAdapterName;  /* filled in by user */
#else
    A_CHAR                      *pWlanAdapterName;  /* filled in by user */
#endif /* UNDER_CE */
    int                         FilterEnabled;      /* filtering is enabled */
    int                         Attached;           /* filter library is attached */
    void                        *pContext;          /* private context for filter library */
    ATHBT_FILTER_CMD_EVENTS_FN  pFilterCmdEvents;   /* function ptr to filter a command or event */
    ATHBT_FILTER_DATA_FN        pFilterAclDataOut;  /* function ptr to filter ACL data out (to radio) */
    ATHBT_FILTER_DATA_FN        pFilterAclDataIn;   /* function ptr to filter ACL data in (from radio) */
    ATHBT_INDICATE_STATE_FN     pIndicateState;     /* function ptr to indicate a state */
} ATH_BT_FILTER_INSTANCE;


/* API MACROS */

#define AthBtFilterHciCommand(instance,packet,length)          \
    if ((instance)->FilterEnabled) {                           \
        (instance)->pFilterCmdEvents((instance)->pContext,     \
                                   ATHBT_HCI_COMMAND,          \
                                   (unsigned char *)(packet),  \
                                   (length));                  \
    }

#define AthBtFilterHciEvent(instance,packet,length)            \
    if ((instance)->FilterEnabled) {                           \
        (instance)->pFilterCmdEvents((instance)->pContext,     \
                                   ATHBT_HCI_EVENT,            \
                                   (unsigned char *)(packet),  \
                                   (length));                  \
    }

#define AthBtFilterHciAclDataOut(instance,packet,length)     \
    if ((instance)->FilterEnabled) {                         \
        (instance)->pFilterAclDataOut((instance)->pContext,  \
                                 (unsigned char *)(packet),  \
                                 (length));                  \
    }

#define AthBtFilterHciAclDataIn(instance,packet,length)      \
    if ((instance)->FilterEnabled) {                         \
        (instance)->pFilterAclDataIn((instance)->pContext,   \
                                 (unsigned char *)(packet),  \
                                 (length));                  \
    }
        
/* if filtering is not desired, the application can indicate the state directly using this
 * macro:
 */
#define AthBtIndicateState(instance,indication,state)           \
    if ((instance)->FilterEnabled) {                            \
        (instance)->pIndicateState((instance)->pContext,        \
                                   (indication),                \
                                   (state),                     \
                                   0);                          \
    }

#ifdef __cplusplus
extern "C" {
#endif

/* API prototypes */
int     AthBtFilter_Attach(ATH_BT_FILTER_INSTANCE *pInstance);
void    AthBtFilter_Detach(ATH_BT_FILTER_INSTANCE *pInstance);

#ifdef __cplusplus
}
#endif

#endif /*ATHBTFILTER_H_*/
