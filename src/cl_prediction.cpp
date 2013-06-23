#include <cl_prediction.h>


namespace avlib
{

CCLPrediction::CCLPrediction(CCLDevice * dev) :
	m_dev(dev),
	m_kernelTransform(NULL)
{
}

CCLPrediction::CCLPrediction(CCLDevice * dev, CImageFormat format) :
	m_dev(dev),
	m_kernelITransform(NULL)
{
	m_last = new CCLImage<float>(dev, format, CL_MEM_READ_ONLY);
}

CCLPrediction::~CCLPrediction()
{
	RELEASE(m_kernelTransform);
	RELEASE(m_kernelITransform);
}

void CCLPrediction::setTransformKernel(cl_program program, const char * kernel)
{
	m_kernelTransform = new ICLKernel(m_dev, program, kernel);
}

void CCLPrediction::setITransformKernel(cl_program program, const char * kernel)
{
	m_kernelITransform = new ICLKernel(m_dev, program, kernel);
}

void CCLPrediction::doTransformPFrame(CImage<float> * pSrc, CImage<float> * pDst, CPredictionInfoTable * pPred)
{
	if(NULL != m_kernelTransform)
	{
		clTransform(m_kernelTransform, pSrc, pDst, pPred);
	}
	else
	{
		CPrediction::doTransformPFrame(pSrc, pDst, pPred);
	}
}

void CCLPrediction::doITransformPFrame(CImage<float> * pSrc, CImage<float> * pDst, CPredictionInfoTable * pPred)
{
	if(NULL != m_kernelITransform)
	{
		CPrediction::doITransformPFrame(pSrc, pDst, pPred);
	}
	else
	{
		CPrediction::doITransformPFrame(pSrc, pDst, pPred);
	}
}

void CCLPrediction::Transform(CImage<float> * pSrc, CImage<float> * pDst, CPredictionInfoTable * pPredInfo, FRAME_TYPE type)
{
	if(FRAME_TYPE_I == type)
	{
		m_IFT->Transform(pSrc, pDst);
	}
	else if(FRAME_TYPE_P == type)
	{
		doPredict(&(*pSrc)[0], pPredInfo);
		doTransformPFrame(pSrc, pDst, pPredInfo);
	}
	else
	{
		throw utils::StringFormatException("Invalid FRAME TYPE (%d)\n", type);
	}
}


void CCLPrediction::ITransform(CImage<float> * pSrc, CImage<float> * pDst, CPredictionInfoTable * pPredInfo, FRAME_TYPE type)
{
	if(pSrc->getFormat() != pDst->getFormat())
	{
		throw utils::StringFormatException("formats not equal\n");
	}
	if(FRAME_TYPE_I == type)
	{
		m_IFIT->Transform(pSrc, pDst);
	}
	else if(FRAME_TYPE_P == type)
	{
		doITransformPFrame(pSrc, pDst, pPredInfo);
	}
	else
	{
		throw utils::StringFormatException("Unknown FRAME_TYPE (%d)\n", type);
	}
	*m_last = *pDst;
}

void CCLPrediction::clTransform(ICLKernel * kernel, CImage<float> * pSrc, CImage<float> * pDst, CPredictionInfoTable * pPred)
{
	CCLImage<float> * clSrc = dynamic_cast<CCLImage<float>*>(pSrc);
	CCLImage<float> * clDst = dynamic_cast<CCLImage<float>*>(pDst);
	CCLImage<float> * clLast = dynamic_cast<CCLImage<float>*>(m_last);
	CCLPredictionInfoTable * clPred = dynamic_cast<CCLPredictionInfoTable*>(pPred);
	if(NULL == clSrc || NULL == clDst || NULL == clPred || NULL == clLast)
	{
		throw utils::StringFormatException("clTransform(): src or dst is not CCLImage<float>\n");
	}
	int predHeight = clPred->getHeight();
	int predWidth = clPred->getWidth();
	for(int k=0;k<clSrc->getComponents();k++)
	{
		int height = (*clSrc)[k].getHeight();
		int width = (*clSrc)[k].getWidth();
		int scale = 16/clSrc->getScale(k);
		cl_mem srcMem = clSrc->getCLComponent(k).getCLMem(true);
		cl_mem dstMem = clDst->getCLComponent(k).getCLMem(true);
		cl_mem lastMem = clLast->getCLComponent(k).getCLMem(true);
		cl_mem predMem = clPred->getCLMem(true);
		size_t global_work_size[2];
		global_work_size[0] = height;
		global_work_size[1] = width;

		kernel->SetArg(0, sizeof(srcMem), &srcMem);
		kernel->SetArg(1, sizeof(dstMem), &dstMem);
		kernel->SetArg(2, sizeof(lastMem), &lastMem);
		kernel->SetArg(3, sizeof(predMem), &predMem);
		kernel->SetArg(4, sizeof(width), &width);
		kernel->SetArg(5, sizeof(predWidth), &predWidth);
		kernel->SetArg(6, sizeof(scale), &scale);

		kernel->EnqueueNDRangeKernel(2, global_work_size, NULL, 0, NULL, NULL);
		kernel->Finish();
	}
}

}
