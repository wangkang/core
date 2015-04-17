#ifndef _RENDERER_OUPUT_EMF_H
#define _RENDERER_OUPUT_EMF_H


#include "../../../graphics/IRenderer.h"
#include "../../../graphics/structures.h"
#include "../../../graphics/Image.h"
#include "../../../raster/ImageFileFormatChecker.h"
#include "../../../raster/BgraFrame.h"

#include "EmfOutputDevice.h"
#include "EmfFile.h"

namespace MetaFile
{
	class CEmfRendererOutput : public CEmfOutputDevice
	{

	public:
		CEmfRendererOutput(CEmfFile *pFile, IRenderer *pRenderer, double dX, double dY, double dWidth, double dHeight)
		{
			m_pEmfFile = pFile;

			m_dX = dX;
			m_dY = dY;
			m_dW = dWidth;
			m_dH = dHeight;

			m_pRenderer = NULL;

			if (!pRenderer)
				return;

			m_pRenderer = pRenderer;

			TEmfRectL* pBounds = m_pEmfFile->GetDCBounds();
			long lL = pBounds->lLeft;
			long lR = pBounds->lRight;
			long lT = pBounds->lTop;
			long lB = pBounds->lBottom;

			m_dScaleX = (lR - lL <= 0) ? 1 : m_dW / (double)(lR - lL);
			m_dScaleY = (lB - lT <= 0) ? 1 : m_dH / (double)(lB - lT);

			m_bStartedPath = false;
		}
		~CEmfRendererOutput()
		{
		}

		void Begin()
		{
		}
		void End()
		{
			CheckEndPath();
		}

		void DrawBitmap(long lX, long lY, long lW, long lH, BYTE* pBuffer, unsigned long ulWidth, unsigned long ulHeight)
		{
			CheckEndPath();

			UpdateTransform();

			Aggplus::CImage oImage;
			BYTE* pBufferPtr = new BYTE[4 * ulWidth * ulHeight];
			oImage.Create(pBufferPtr, ulWidth, ulHeight, 4 * ulWidth);

			for (int nIndex = 0, nSize = 4 * ulWidth * ulHeight; nIndex < nSize; nIndex += 4)
			{
				pBufferPtr[0] = (unsigned char)pBuffer[nIndex + 0];
				pBufferPtr[1] = (unsigned char)pBuffer[nIndex + 1];
				pBufferPtr[2] = (unsigned char)pBuffer[nIndex + 2];
				pBufferPtr[3] = (unsigned char)pBuffer[nIndex + 3];
				pBufferPtr += 4;
			}

			double dX = TransX(lX);
			double dY = TransX(lY);

			double dX1 = TransX(lX + lW);
			double dY1 = TransY(lY + lH);

			m_pRenderer->DrawImage(&oImage, dX, dY, dX1 - dX, dY1 - dY);
		}
		void DrawText(const wchar_t* wsText, unsigned long ulCharsCount, long lX, long lY)
		{
			CheckEndPath();

			UpdateTransform();

			CEmfDC* pDC = m_pEmfFile->GetDC();
			if (!pDC)
				return;

			CEmfLogFont* pFont = pDC->GetFont();
			if (!pFont)
				return;

			TEmfLogFont* pLogFont = &pFont->LogFontEx.LogFont;

			TEmfRectL* pBounds = &m_pEmfFile->m_oHeader.oBounds;
			TEmfRectL* pFrame  = &m_pEmfFile->m_oHeader.oFrame;

			double dFontHeight = TransY(std::abs(pLogFont->Height)) / 25.4 * 72;
			std::wstring wsFaceName((const wchar_t*)pLogFont->FaceName);
			m_pRenderer->put_FontName(wsFaceName);
			m_pRenderer->put_FontSize(dFontHeight);

			long lStyle = 0;
			if (pLogFont->Weight > 550)
				lStyle |= 0x01;
			if (pLogFont->Italic)
				lStyle |= 0x02;
			if (pLogFont->Underline)
				lStyle |= (1 << 2);
			if (pLogFont->StrikOut)
				lStyle |= (1 << 7);

			m_pRenderer->put_FontStyle(lStyle);

			TEmfColor oTextColor = pDC->GetTextColor();

			// ��������� ���� ������
			m_pRenderer->put_BrushType(c_BrushTypeSolid);
			m_pRenderer->put_BrushColor1(METAFILE_RGBA(oTextColor.r, oTextColor.g, oTextColor.b));
			m_pRenderer->put_BrushAlpha1(255);

			double dTheta = -((((double)pLogFont->Escapement) / 10) * M_PI / 180);

			double dCosTheta = (float)cos(dTheta);
			double dSinTheta = (float)sin(dTheta);

			float fL = 0, fT = 0, fW = 0, fH = 0;
			float fUndX1 = 0, fUndY1 = 0, fUndX2 = 0, fUndY2 = 0, fUndSize = 1;
			CFontManager* pFontManager = m_pEmfFile->m_pFontManager;
			if (pFontManager)
			{
				pFontManager->LoadFontByName(wsFaceName, dFontHeight, lStyle, 72, 72);
				pFontManager->LoadString1(wsText, 0, 0);
				TBBox oBox = pFontManager->MeasureString2();
				fL = oBox.fMinX;
				fT = oBox.fMinY;
				fW = oBox.fMaxX - oBox.fMinX;
				fH = oBox.fMaxY - oBox.fMinY;
				pFontManager->GetUnderline(&fUndX1, &fUndY1, &fUndX2, &fUndY2, &fUndSize);

				double fKoef = 25.4 / 72;

				fL *= (float)fKoef;
				fT *= (float)fKoef;
				fW *= (float)fKoef;
				fH *= (float)fKoef;

				fUndX1 *= (float)fKoef; fUndY1 *= (float)fKoef;
				fUndX2 *= (float)fKoef; fUndY2 *= (float)fKoef;
				fUndSize *= (float)fKoef / 2;
			}

			double dX = TransX(lX);
			double dY = TransX(lY);

			// ������ ��������� ����� ������
			unsigned long ulTextAlign = pDC->GetTextAlign();
			if (ulTextAlign & TA_BASELINE)
			{
				// ������ �� ������
			}
			else if (ulTextAlign & TA_BOTTOM)
			{
				float fTemp = -(-fT + fH);

				dX += -fTemp * dSinTheta;
				dY +=  fTemp * dCosTheta;
			}
			else // if (ulTextAlign & TA_TOP)
			{
				float fTemp = -fT;

				dX += -fTemp * dSinTheta;
				dY +=  fTemp * dCosTheta;
			}

			if (ulTextAlign & TA_CENTER)
			{
				dX += -fW / 2 * dCosTheta;
				dY += -fW / 2 * dSinTheta;
			}
			else if (ulTextAlign & TA_RIGHT)
			{
				dX += -fW * dCosTheta;
				dY += -fW * dSinTheta;
			}
			else //if (ulTextAlign & TA_LEFT)
			{
				// ������ �� ������
			}

			if (pLogFont->Underline)
			{
				fUndX1 += (float)dX;
				fUndX2 += (float)dX;
				fUndY1 += (float)dY;
				fUndY2 += (float)dY;
			}

			bool bChangeCTM = false;
			if (0 != pLogFont->Escapement)
			{
				// TODO: ��� ���������� ������ �������� shEscapement, ��� ����� ����������� �������� Orientation
				m_pRenderer->SetTransform(dCosTheta, dSinTheta, -dSinTheta, dCosTheta, dX - dX * dCosTheta + dY * dSinTheta, dY - dX * dSinTheta - dY * dCosTheta);
				bChangeCTM = true;
			}

			// ��� ������ �������� ��� ������
			if (OPAQUE == pDC->GetBgMode())
			{
				TEmfColor oBgColor = pDC->GetBgColor();
				m_pRenderer->put_BrushType(c_BrushTypeSolid);
				m_pRenderer->put_BrushColor1(255);
				m_pRenderer->put_BrushAlpha1(METAFILE_RGBA(oBgColor.r, oBgColor.g, oBgColor.b));

				m_pRenderer->BeginCommand(c_nPathType);
				m_pRenderer->PathCommandStart();
				m_pRenderer->PathCommandMoveTo(dX + fL, dY + fT);
				m_pRenderer->PathCommandLineTo(dX + fL + fW, dY + fT);
				m_pRenderer->PathCommandLineTo(dX + fL + fW, dY + fT + fH);
				m_pRenderer->PathCommandLineTo(dX + fL, dY + fT + fH);
				m_pRenderer->PathCommandClose();
				m_pRenderer->DrawPath(c_nWindingFillMode);
				m_pRenderer->EndCommand(c_nPathType);
				m_pRenderer->PathCommandStart();
			}

			// �������� ������������� 
			if (pLogFont->Underline)
			{
				m_pRenderer->put_PenSize((double)fUndSize);
				m_pRenderer->put_PenLineEndCap(0);
				m_pRenderer->put_PenLineStartCap(0);

				m_pRenderer->BeginCommand(c_nPathType);
				m_pRenderer->PathCommandStart();
				m_pRenderer->PathCommandMoveTo(fUndX1, fUndY1);
				m_pRenderer->PathCommandLineTo(fUndX2, fUndY2);
				m_pRenderer->DrawPath(c_nStroke);
				m_pRenderer->EndCommand(c_nPathType);
				m_pRenderer->PathCommandEnd();
			}

			// ������ ��� �����
			m_pRenderer->CommandDrawText(wsText, dX, dY, 0, 0, 0);

			if (bChangeCTM)
				m_pRenderer->ResetTransform();
		}

		void StartPath()
		{
			UpdateTransform();

			m_lDrawPathType = -1;
			if (true == UpdateBrush())
			{
				CEmfDC* pDC = m_pEmfFile->GetDC();
				if (ALTERNATE == pDC->GetFillMode())
					m_lDrawPathType = c_nEvenOddFillMode;
				else// if (WINDING == pDC->GetFillMode())
					m_lDrawPathType = c_nWindingFillMode;
			}

			if (true == UpdatePen())
			{
				if (-1 == m_lDrawPathType)
					m_lDrawPathType = c_nStroke;
				else
					m_lDrawPathType |= c_nStroke;
			}

			m_pRenderer->BeginCommand(c_nPathType);
			m_pRenderer->PathCommandStart();

			m_bStartedPath = true;
		}
		void MoveTo(long lX, long lY)
		{
			CheckStartPath();

			double dX = TransX(lX);
			double dY = TransY(lY);
			m_pRenderer->PathCommandMoveTo(dX, dY);
		}
		void LineTo(long lX, long lY)
		{
			CheckStartPath();

			double dX = TransX(lX);
			double dY = TransY(lY);
			m_pRenderer->PathCommandLineTo(dX, dY);
		}
		void CurveTo(long lX1, long lY1, long lX2, long lY2, long lXe, long lYe)
		{
			CheckStartPath();

			double dX1 = TransX(lX1), dX2 = TransX(lX2), dXe = TransX(lXe);
			double dY1 = TransY(lY1), dY2 = TransY(lY2), dYe = TransY(lYe);
			m_pRenderer->PathCommandCurveTo(dX1, dY1, dX2, dY2, dXe, dYe);
		}
		void ArcTo(long lLeft, long lTop, long lRight, long lBottom, double dStart, double dSweep)
		{
			CheckStartPath();

			double dL = TransX(lLeft);
			double dT = TransY(lTop);
			double dR = TransX(lRight);
			double dB = TransY(lBottom);

			m_pRenderer->PathCommandArcTo(dL, dT, dR - dL, dB - dT, dStart, dSweep);
		}
		void ClosePath()
		{
			CheckStartPath();

			m_pRenderer->PathCommandClose();
		}
		void DrawPath(long lType = 0)
		{
			if (lType <= 0)
			{
				if (-1 != m_lDrawPathType)
					m_pRenderer->DrawPath(m_lDrawPathType);
			}
			else if (-1 != m_lDrawPathType)
			{
				bool bStroke = lType & 1 ? true : false;
				bool bFill   = lType & 2 ? true : false;

				long m_lEndType = -1;

				if (bStroke && m_lDrawPathType & c_nStroke)
					m_lEndType = c_nStroke;
				else
					m_lEndType = c_nStroke;
				
				if (bFill)
				{
					if (m_lDrawPathType & c_nWindingFillMode)
						m_lEndType = (-1 == m_lDrawPathType ? c_nWindingFillMode : m_lDrawPathType | c_nWindingFillMode);
					else if (m_lDrawPathType & c_nEvenOddFillMode)
						m_lEndType = (-1 == m_lDrawPathType ? c_nEvenOddFillMode : m_lDrawPathType | c_nEvenOddFillMode);
				}

				if (-1 != m_lEndType)
					m_pRenderer->DrawPath(m_lEndType);
			}
		}
		void EndPath()
		{
			m_pRenderer->EndCommand(c_nPathType);
			m_pRenderer->PathCommandEnd();

			m_bStartedPath = false;
		}
		void UpdateDC()
		{
			CheckEndPath();
		}

	private:

		void CheckStartPath()
		{
			if (!m_bStartedPath)
			{
				StartPath();
			}
		}
		void CheckEndPath()
		{
			if (m_bStartedPath)
			{
				DrawPath();
				EndPath();
			}
		}

		double TransX(long _lX)
		{
			long lX = m_pEmfFile->TranslateY(_lX);
			TEmfRectL* pBounds = m_pEmfFile->GetDCBounds();
			long lL = pBounds->lLeft;
			return m_dScaleX * (double)(lX - lL) + m_dX;
		}
		double TransY(long _lY)
		{
			long lY = m_pEmfFile->TranslateY(_lY);
			TEmfRectL* pBounds = m_pEmfFile->GetDCBounds();
			long lT = pBounds->lTop;
			return m_dScaleY * (double)(lY - lT) + m_dY;
		}

		bool UpdateBrush()
		{
			CEmfDC* pDC = m_pEmfFile->GetDC();
			if (!pDC)
				return false;

			CEmfLogBrushEx* pBrush = pDC->GetBrush();
			if (!pBrush)
				return false;

			long lColor = METAFILE_RGBA(pBrush->Color.r, pBrush->Color.g, pBrush->Color.b);

			if (BS_NULL == pBrush->BrushStyle)
				return false;			
			else if (BS_DIBPATTERN == pBrush->BrushStyle)
			{
				m_pRenderer->put_BrushType(c_BrushTypeTexture);
				m_pRenderer->put_BrushTextureMode(c_BrushTextureModeTile);
				m_pRenderer->put_BrushTexturePath(pBrush->DibPatternPath);
			}
			else //if (BS_SOLID == pBrush->BrushStyle)
			{
				m_pRenderer->put_BrushColor1(lColor);
				m_pRenderer->put_BrushAlpha1(pBrush->BrushAlpha);
				m_pRenderer->put_BrushType(c_BrushTypeSolid);
			}

			return true;
		}
		void UpdateTransform()
		{
			CEmfDC* pDC = m_pEmfFile->GetDC();
			if (!pDC)
				return;

			double dKoefX = m_dScaleX;
			double dKoefY = m_dScaleY;

			TEmfXForm* pMatrix = pDC->GetTransform();
			m_pRenderer->ResetTransform();
			m_pRenderer->SetTransform(pMatrix->M11, pMatrix->M12 * dKoefY / dKoefX, pMatrix->M21 * dKoefX / dKoefY, pMatrix->M22, pMatrix->Dx * dKoefX, pMatrix->Dy * dKoefY);
		}
		bool UpdatePen()
		{
			CEmfDC* pDC = m_pEmfFile->GetDC();
			if (!pDC)
				return false;

			CEmfLogPen* pPen = pDC->GetPen();
			if (!pPen)
				return false;

			long lColor = METAFILE_RGBA(pPen->Color.r, pPen->Color.g, pPen->Color.b);

			// TODO: dWidth ������� ��� �� ����� PS_GEOMETRIC � ����� ���������
			double dWidth = pPen->Width * m_dScaleX * pDC->GetPixelWidth();
			if (dWidth <= 0.01)
				dWidth = 0;

			unsigned long ulPenType   = pPen->PenStyle & PS_TYPE_MASK;
			unsigned long ulPenEndCap = pPen->PenStyle & PS_ENDCAP_MASK;
			unsigned long ulPenJoin   = pPen->PenStyle & PS_JOIN_MASK;
			unsigned long ulPenStyle  = pPen->PenStyle & PS_STYLE_MASK;

			BYTE nCapStyle = 0;
			if (0 == ulPenEndCap)
				nCapStyle = 2;
			else if (1 == ulPenEndCap)
				nCapStyle = 1;
			else if (2 == ulPenEndCap)
				nCapStyle = 0;

			BYTE nJoinStyle = 0;
			if (0 == ulPenJoin)
				nJoinStyle = 2;
			else if (1 == ulPenJoin)
				nJoinStyle = 1;
			else if (2 == ulPenJoin)
				nJoinStyle = 2;

			double dMiterLimit = pDC->GetMiterLimit() * m_dScaleX;

			// TODO: ��������� ���� ���� ���������� ����������� � ������� ����������� ���������, ������� �� ������ ��� ���� PS_SOLID.
			// TODO: ����������� PS_USERSTYLE
			BYTE nDashStyle;
			if (PS_ALTERNATE == ulPenStyle || PS_USERSTYLE == ulPenStyle || PS_INSIDEFRAME == ulPenStyle)
				nDashStyle = (BYTE)PS_SOLID;
			else if (PS_NULL != ulPenStyle)
				nDashStyle = (BYTE)ulPenStyle;

			m_pRenderer->put_PenDashStyle(nDashStyle);
			m_pRenderer->put_PenLineJoin(nJoinStyle);
			m_pRenderer->put_PenLineStartCap(nCapStyle);
			m_pRenderer->put_PenLineEndCap(nCapStyle);
			m_pRenderer->put_PenColor(lColor);
			m_pRenderer->put_PenSize(dWidth);
			m_pRenderer->put_PenAlpha(255);
			m_pRenderer->put_PenMiterLimit(dMiterLimit);

			//// TO DO: � ������� ����������� AVSRenderer, ��������� ������ ushROPMode
			////        ����������� ����������. ������ ��� ������ �������� ����� ������������
			////        ��� �������� Pen'a, � ��� ��� ������ ��� ������ ����������� ��� ������.

			//switch (pDC->ushROPMode)
			//{
			//	case R2_BLACK:   m_pRenderer->put_PenColor(0); break;
			//	case R2_NOP:     m_pRenderer->put_PenAlpha(0); break;
			//	case R2_COPYPEN: break;
			//	case R2_WHITE:   m_pRenderer->put_PenColor(METAFILE_RGBA(255, 255, 255)); break;
			//}

			if (PS_NULL == ulPenStyle)
				return false;

			return true;
		}

	private:

		IRenderer* m_pRenderer;
		long       m_lDrawPathType;
		double     m_dX;      // ���������� ������ �������� ����
		double     m_dY;      //
		double     m_dW;      // 
		double     m_dH;      // 
		double     m_dScaleX; // ������������ ������/����������, ����� 
		double     m_dScaleY; // �������������� �������� ���� ������ ��������.
		CEmfFile*  m_pEmfFile;
		bool       m_bStartedPath;
	};
}
#endif // _RENDERER_OUPUT_EMF_H