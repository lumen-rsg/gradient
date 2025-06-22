//
// Created by cv2 on 6/14/25.
//

#ifndef DOWNLOADHELPER_H
#define DOWNLOADHELPER_H

#pragma once

#include <string>
#include <mutex>
#include <curl/curl.h>

namespace gradient {

/// Context for a single download’s progress.
struct DownloadContext {
    int index;
    int total;
    std::string name;
    std::mutex* printMutex;
};

/// libcurl write callback (just dump into file)
static size_t writeFile(void* ptr, size_t size, size_t nmemb, void* stream) {
    FILE* f = static_cast<FILE*>(stream);
    return fwrite(ptr, size, nmemb, f);
}

/// libcurl xferinfo callback to update progress
static int progressCallback(void* clientp,
                            curl_off_t dltotal,
                            curl_off_t dlnow,
                            curl_off_t ultotal,
                            curl_off_t ulnow)
{
    auto ctx = static_cast<DownloadContext*>(clientp);
    std::lock_guard<std::mutex> lk(*ctx->printMutex);

    int barWidth = 40;
    double fraction = dltotal > 0 ? (double)dlnow / (double)dltotal : 0.0;
    int pos = (int)(fraction * barWidth);
    int percent = (int)(fraction * 100.0);

    // Carriage-return overwrite single line:
    printf("\r  ↓ [%d/%d] %-20s [", ctx->index, ctx->total, ctx->name.c_str());
    for (int i = 0; i < barWidth; ++i) {
        fputc(i < pos ? '=' : ' ', stdout);
    }
    printf("] %3d%%", percent);
    fflush(stdout);

    return 0; // return non-zero to abort
}

/// Download a single URL to the given filesystem path, showing progress.
///
/// @returns true on success, false on error.
static bool downloadWithCurl(const std::string& url,
                             const std::string& outPath,
                             DownloadContext& ctx)
{
    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) return false;

    CURL* curl = curl_easy_init();
    if (!curl) { fclose(f); return false; }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

    // enable progress callback
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    // timeouts & retries
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10L);

    CURLcode res = curl_easy_perform(curl);

    // finish the line
    {
        std::lock_guard<std::mutex> lk(*ctx.printMutex);
        if (res == CURLE_OK) {
            printf("\r  ✔ [%d/%d] %-20s [", ctx.index, ctx.total, ctx.name.c_str());
            for (int i = 0; i < 40; ++i) fputc('=', stdout);
            printf("] 100%%\n");
        } else {
            printf("\r  ✖ [%d/%d] %-20s download failed: %s\n",
                   ctx.index, ctx.total,
                   ctx.name.c_str(),
                   curl_easy_strerror(res));
        }
        fflush(stdout);
    }

    curl_easy_cleanup(curl);
    fclose(f);
    return res == CURLE_OK;
}

} // namespace anemo

#endif //DOWNLOADHELPER_H
