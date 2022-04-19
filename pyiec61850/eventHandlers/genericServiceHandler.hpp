#ifndef PYIEC61850_GENERICSERVICEHANDLER_HPP
#define PYIEC61850_GENERICSERVICEHANDLER_HPP

#include "eventHandler.hpp"

class GenericServiceHandlerForPython: public EventHandler {
    public:
        virtual ~GenericServiceHandlerForPython();

        virtual void setReceivedData(void *i_data_p)
        {
            IedClientError *l_my_data_p = static_cast<IedClientError*>(i_data_p);
            _libiec61850_client_error = *l_my_data_p;        
        }

        void setInvokeId(uint32_t i_invoke_id)
        {
            _invoke_id = i_invoke_id;
        }

        int _libiec61850_client_error;
    
    private:
        uint32_t _invoke_id;
};

class GenericServiceCallManagerForPython {
    
    public:
        ~GenericServiceCallManagerForPython()
        {}

        static bool registerServiceHandler(uint32_t i_invoke_id, GenericServiceHandlerForPython* i_generic_service_handler)
        {
            // Preconditions
            if (!i_invoke_id) {
                fprintf(stderr, "GenericServiceCallManagerForPython::registerServiceCall() failed: invalid invokeId\n");
                return false;
            }
            if (m_call_map.end() != m_call_map.find(i_invoke_id)) {
                fprintf(stderr, "GenericServiceCallManagerForPython::registerServiceCall() failed: the handler is already registered\n");
                return false;
            }

            m_call_map[i_invoke_id] = i_generic_service_handler;

            return true;
        } 

         static void unRegisterServiceHandler(uint32_t i_invoke_id)
        {
            std::map<uint32_t, GenericServiceHandlerForPython*>::iterator l_it = m_call_map.find(i_invoke_id);

            if (m_call_map.end() != l_it) {
                m_call_map.erase(l_it);
            }
            else {
                fprintf(stderr, "GenericServiceCallManagerForPython::unRegisterServiceHandler() failed: handler is not registered\n");
            }
        } 

        static void triggerServiceCallback(uint32_t i_invoke_id, void* i_parameter, IedClientError i_err) 
        {
            // Initialization is a no-op if already initialized. PyEval_InitThreads has no effect for python > 3.6
            Py_Initialize();
            PyEval_InitThreads();

            // Ensure Python GIL during python handling
            PyThreadStateLock PyThreadLock;

            // Get handler instance using invokeId
            GenericServiceHandlerForPython *l_handler_p = GenericServiceCallManagerForPython::findHandler(i_invoke_id);

            if (l_handler_p) {
                l_handler_p->setReceivedData(&i_err);
                l_handler_p->setInvokeId(i_invoke_id);
                l_handler_p->trigger();
            }
            else {
                fprintf(stderr, "GenericServiceCallManagerForPython::triggerServiceCallback() failed: GenericServiceHandlerForPython is undefined\n");
            }
        }

    private:
        static std::map<uint32_t, GenericServiceHandlerForPython*> m_call_map;

        static GenericServiceHandlerForPython* findHandler(u_int32_t i_invoke_id)
        {
            GenericServiceHandlerForPython *o_found_event_handler_p = nullptr;
            std::map<uint32_t, GenericServiceHandlerForPython*>::iterator l_it = m_call_map.find(i_invoke_id);

            if (m_call_map.end() != l_it) {
                o_found_event_handler_p = l_it->second;
            }

            return o_found_event_handler_p;
        }
};

GenericServiceHandlerForPython::~GenericServiceHandlerForPython()
{                        
    GenericServiceCallManagerForPython::unRegisterServiceHandler(_invoke_id);
}

static void 
setRCBValuesAsyncWrapper(IedConnection con, IedClientError* error, ClientReportControlBlock rcb, uint32_t parametersMask, 
        bool singleRequest, GenericServiceHandlerForPython* handler_ptr)
{
    uint32_t invokeId = IedConnection_setRCBValuesAsync(con, error, rcb, parametersMask, singleRequest, GenericServiceCallManagerForPython::triggerServiceCallback, NULL);
    GenericServiceCallManagerForPython::registerServiceHandler(invokeId, handler_ptr);
}

#endif
