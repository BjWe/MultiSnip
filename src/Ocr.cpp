#include "Ocr.h"
#include <unknwn.h>
#include <memorybuffer.h>
#include <vector>
#include <thread>
#include <algorithm>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>

using namespace winrt;
using namespace winrt::Windows::Globalization;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;

namespace {

struct Job {
    std::vector<BYTE> px;       // BGRA, top-down
    int w = 0, h = 0;
    HWND notify = nullptr;
    UINT msg = 0;
};

// Bitmap-Daten aus einem DIB holen; kleine Bilder werden fuer bessere Trefferquote verdoppelt.
bool GrabPixels(HBITMAP bmp, int w, int h, Job& job)
{
    const int kMax = 9000;                                  // OcrEngine::MaxImageDimension ist 10000
    int scale = (w < 1400 && h < 1400 && w * 2 <= kMax && h * 2 <= kMax) ? 2 : 1;
    int dw = std::min(w * scale, kMax), dh = std::min(h * scale, kMax);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = dw;
    bi.bmiHeader.biHeight = -dh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP tmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!tmp) return false;
    HDC dst = CreateCompatibleDC(NULL), src = CreateCompatibleDC(NULL);
    HGDIOBJ od = SelectObject(dst, tmp), os = SelectObject(src, bmp);
    SetStretchBltMode(dst, HALFTONE);
    SetBrushOrgEx(dst, 0, 0, NULL);
    StretchBlt(dst, 0, 0, dw, dh, src, 0, 0, w, h, SRCCOPY);
    GdiFlush();
    SelectObject(src, os); SelectObject(dst, od);
    DeleteDC(src); DeleteDC(dst);
    job.px.assign((BYTE*)bits, (BYTE*)bits + (size_t)dw * dh * 4);
    for (size_t i = 3; i < job.px.size(); i += 4) job.px[i] = 0xFF;   // deckend
    job.w = dw; job.h = dh;
    DeleteObject(tmp);
    return true;
}

OcrEngine MakeEngine()
{
    OcrEngine e = OcrEngine::TryCreateFromUserProfileLanguages();
    if (!e) {
        auto langs = OcrEngine::AvailableRecognizerLanguages();
        if (langs.Size() > 0) e = OcrEngine::TryCreateFromLanguage(langs.GetAt(0));
    }
    return e;
}

void Worker(Job* jobPtr)
{
    std::unique_ptr<Job> job(jobPtr);
    OcrStatus status = OcrStatus::Failed;
    std::wstring* result = nullptr;
    try {
        init_apartment(apartment_type::multi_threaded);
        OcrEngine engine = MakeEngine();
        if (!engine) status = OcrStatus::NoLanguage;
        else {
            SoftwareBitmap sb(BitmapPixelFormat::Bgra8, job->w, job->h, BitmapAlphaMode::Ignore);
            {
                BitmapBuffer buf = sb.LockBuffer(BitmapBufferAccessMode::Write);
                auto ref = buf.CreateReference();
                auto access = ref.as<::Windows::Foundation::IMemoryBufferByteAccess>();
                BYTE* data = nullptr; UINT32 cap = 0;
                check_hresult(access->GetBuffer(&data, &cap));
                BitmapPlaneDescription pd = buf.GetPlaneDescription(0);
                for (int y = 0; y < job->h; y++)
                    memcpy(data + pd.StartIndex + (size_t)y * pd.Stride, job->px.data() + (size_t)y * job->w * 4, (size_t)job->w * 4);
            }
            OcrResult res = engine.RecognizeAsync(sb).get();
            std::wstring text;
            for (auto line : res.Lines()) {
                std::wstring l;
                for (auto word : line.Words()) {
                    if (!l.empty()) l += L' ';
                    l += word.Text().c_str();
                }
                if (!text.empty()) text += L"\r\n";
                text += l;
            }
            if (text.empty()) status = OcrStatus::NoText;
            else { status = OcrStatus::Ok; result = new std::wstring(std::move(text)); }
        }
    }
    catch (...) { status = OcrStatus::Failed; }
    uninit_apartment();
    if (!PostMessageW(job->notify, job->msg, (WPARAM)status, (LPARAM)result)) delete result;
}

} // namespace

bool Ocr_Available()
{
    try { return OcrEngine::AvailableRecognizerLanguages().Size() > 0; }
    catch (...) { return false; }
}

std::wstring Ocr_LanguageName()
{
    try {
        OcrEngine e = MakeEngine();
        if (!e) return L"";
        return e.RecognizerLanguage().DisplayName().c_str();
    }
    catch (...) { return L""; }
}

bool Ocr_Start(HBITMAP bmp, int w, int h, HWND notify, UINT msg)
{
    if (!bmp || w <= 0 || h <= 0) return false;
    auto job = std::make_unique<Job>();
    if (!GrabPixels(bmp, w, h, *job)) return false;
    job->notify = notify;
    job->msg = msg;
    try {
        std::thread(Worker, job.release()).detach();
    }
    catch (...) { return false; }
    return true;
}
