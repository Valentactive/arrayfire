#pragma once

#include <af/defines.h>
#include <kernel_headers/resize.hpp>
#include <cl.hpp>
#include <ctx.hpp>
#include <traits.hpp>
#include <sstream>
#include <string>
#include <dispatch.hpp>

typedef struct
{
    dim_type dim[4];
} dims_t;


using cl::Buffer;
using cl::Program;
using cl::make_kernel;
using cl::EnqueueArgs;
using cl::NDRange;
using std::string;

namespace opencl
{
    namespace kernel
    {
        static const dim_type TX = 16;
        static const dim_type TY = 16;

        template<typename T, af_interp_type method>
        void resize(Buffer out, const dim_type odim0, const dim_type odim1,
              const Buffer in,  const dim_type idim0, const dim_type idim1,
              const dim_type channels, const dim_type *ostrides, const dim_type *istrides,
              const dim_type offset)
        {
            Program::Sources setSrc;
            setSrc.emplace_back(resize_cl, resize_cl_len);
            Program prog(getCtx(0), setSrc);

            std::ostringstream options;
            options << " -D T="        << dtype_traits<T>::getName()
                    << " -D dim_type=" << dtype_traits<dim_type>::getName();

            switch(method) {
                case AF_INTERP_NEAREST:
                    options << " -D INTERP=NEAREST";
                    break;
                case AF_INTERP_BILINEAR:
                    options << " -D INTERP=BILINEAR";
                    break;
                default:
                    break;
            }

            prog.build(options.str().c_str());

            cl_int err = 0;
            auto resizeOp = make_kernel<Buffer, const dim_type, const dim_type,
                                  const Buffer, const dim_type, const dim_type,
                                  const dims_t, const dims_t, const dim_type,
                                  const unsigned, const float, const float>
                                  (prog, "resize_kernel", &err);

            NDRange local(TX, TY, 1);

            unsigned blocksPerMatX = divup(odim0, local[0]);
            unsigned blocksPerMatY = divup(odim1, local[1]);
            NDRange global(local[0] * blocksPerMatX * channels,
                           local[1] * blocksPerMatY,
                           1);

            double xd = (double)idim0 / (double)odim0;
            double yd = (double)idim1 / (double)odim1;

            float xf = (float)xd, yf = (float)yd;

            dims_t _ostrides = {{ostrides[0], ostrides[1], ostrides[2], ostrides[3]}};
            dims_t _istrides = {{istrides[0], istrides[1], istrides[2], istrides[3]}};

            resizeOp(EnqueueArgs(getQueue(0), global, local),
                     out, odim0, odim1, in, idim0, idim1,
                     _ostrides, _istrides, offset, blocksPerMatX, xf, yf);
        }
    }
}