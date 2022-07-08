/**
 * @file ISBootloaderThread.cpp
 * @author Dave Cutting (davidcutting42@gmail.com)
 * @brief Inertial Sense routines for updating embedded systems in parallel
 * 
 */

/*
MIT LICENSE

Copyright (c) 2014-2022 Inertial Sense, Inc. - http://inertialsense.com

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ISBootloaderThread.h"
#include "ISBootloaderDFU.h"

vector<is_device_context*> ISBootloader::ctx;

size_t ISBootloader::get_num_devices(vector<string>& comPorts)
{
    size_t ret = 0;

    is_dfu_list dfu_list;
    is_dfu_list_devices(&dfu_list);

    ret += dfu_list.present;
    ret += comPorts.size();

    return ret;
}

static bool any_in_progress(vector<is_device_context*>& ctx)
{
    for(size_t i = 0; i < ctx.size(); i++)
    {
        if(ctx[i]->thread != NULL)
        {
            return true;
        }
    }

    return false;
}

is_operation_result ISBootloader::update(
    vector<string>&             comPorts,   // ISB and SAM-BA and APP
    vector<string>&             uids,       // DFU only
    int                         baudRate,
    const char*                 firmware,
    pfnBootloadProgress         uploadProgress, 
    pfnBootloadProgress         verifyProgress, 
    pfnBootloadStatus           infoProgress,
    void*                       user_data,
    void						(*waitAction)()
)
{
    is_dfu_list dfu_list;
    size_t i, j;

    for(i = 0; i < ctx.size(); i++)
    {
        is_destroy_context(ctx[i]);
    }
    ctx.clear();

    if(libusb_init(NULL) < 0) return IS_OP_ERROR;
    is_dfu_list_devices(&dfu_list);

    for(i = 0; i < dfu_list.present; i++)
    {	// Create contexts for devices in DFU mode
        for(j = 0; j < uids.size(); j++)
        {
            if(strncmp(dfu_list.id[i].uid, uids[j].c_str(), IS_DFU_UID_MAX_SIZE) == 0)
            {
                is_device_handle handle;
                memset(&handle, 0, sizeof(is_device_handle));
                handle.status = IS_HANDLE_TYPE_LIBUSB;
                memcpy(&handle.dfu.uid, &dfu_list.id[i].uid, IS_DFU_UID_MAX_SIZE);
                handle.dfu.vid = dfu_list.id[i].vid;
                handle.dfu.pid = dfu_list.id[i].pid;
                handle.dfu.sn = 0;
                ctx.push_back(is_create_context(
                    &handle,
                    firmware, 
                    uploadProgress, 
                    verifyProgress, 
                    infoProgress,
                    user_data
                ));
            }
        }
    }

    for(i = 0; i < comPorts.size(); i++)
    {	// Create contexts for devices in serial mode (APP/ISB/SAMBA)
        is_device_handle handle;
        memset(&handle, 0, sizeof(is_device_handle));
        strncpy(handle.port_name, comPorts[i].c_str(), 100);
        handle.status = IS_HANDLE_TYPE_SERIAL;
        handle.baud = baudRate;
        ctx.push_back(is_create_context(
            &handle,
            firmware, 
            uploadProgress, 
            verifyProgress, 
            infoProgress,
            user_data
        ));
    }

    for(i = 0; i < ctx.size(); i++)
    {	// Start threads
        ctx[i]->thread = threadCreateAndStart(update_thread, (void*)ctx[i]);
    }

    while (1)
    {	// Wait for threads to finish
        if(ctx.size()) for(i = 0; i < ctx.size(); i++)
        {
            if((ctx[i]->thread != NULL) && !ctx[i]->update_in_progress)
            {
                threadJoinAndFree(ctx[i]->thread);
                ctx[i]->thread = NULL;
            }

            if(!any_in_progress(ctx))
            {
                libusb_exit(NULL);
                return IS_OP_OK;
            }
        }
        else return IS_OP_OK;

        if(waitAction != 0) waitAction();

        SLEEP_MS(10);
    }
    
    libusb_exit(NULL);
    return IS_OP_OK;
}

void ISBootloader::update_thread(void* context)
{
    is_update_flash(context);
}