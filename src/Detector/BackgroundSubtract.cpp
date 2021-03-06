#include "BackgroundSubtract.h"
#include <tuple>

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
BackgroundSubtract::BackgroundSubtract(BGFG_ALGS algType, int channels)
    :
      m_channels(channels),
      m_algType(algType)
{
    config_t config;
    Init(config);
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
BackgroundSubtract::~BackgroundSubtract()
{
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
bool BackgroundSubtract::Init(const config_t& config)
{
    bool failed = true;
    for (; failed;)
    {
        failed = false;

        switch (m_algType)
        {
        case ALG_VIBE:
        {
            std::vector<std::string> paramsConf = { "samples", "pixelNeighbor", "distanceThreshold", "matchingThreshold", "updateFactor" };
            std::vector<int> params = { 20, 1, 20, 3, 16 };
            assert(paramsConf.size() == params.size());

            for (size_t i = 0; i < paramsConf.size(); ++i)
            {
                auto conf = config.find(paramsConf[i]);
                if (conf != config.end())
                {
                    params[i] = std::stoi(conf->second);
                }
            }

            m_modelVibe = std::make_unique<vibe::VIBE>(m_channels, params[0], params[1], params[2], params[3], params[4]);
            break;
        }

#ifdef USE_OCV_BGFG
        case ALG_MOG:
        {
            std::vector<std::string> paramsConf = { "history", "nmixtures", "backgroundRatio", "noiseSigma" };
            auto params = std::make_tuple(100, 3, 0.7, 0);

            for (size_t i = 0; i < paramsConf.size(); ++i)
            {
                auto conf = config.find(paramsConf[i]);
                if (conf != config.end())
                {
                    std::stringstream ss(conf->second);

                    switch (i)
                    {
                    case 0:
                        ss >> std::get<0>(params);
                        break;
                    case 1:
                        ss >> std::get<1>(params);
                        break;
                    case 2:
                        ss >> std::get<2>(params);
                        break;
                    case 3:
                        ss >> std::get<3>(params);
                        break;
                    }
                }
            }

            m_modelOCV = cv::bgsegm::createBackgroundSubtractorMOG(std::get<0>(params), std::get<1>(params), std::get<2>(params), std::get<3>(params));
            break;
        }

        case ALG_GMG:
        {
            std::vector<std::string> paramsConf = { "initializationFrames", "decisionThreshold" };
            auto params = std::make_tuple(50, 0.7);

            for (size_t i = 0; i < paramsConf.size(); ++i)
            {
                auto conf = config.find(paramsConf[i]);
                if (conf != config.end())
                {
                    std::stringstream ss(conf->second);

                    switch (i)
                    {
                    case 0:
                        ss >> std::get<0>(params);
                        break;
                    case 1:
                        ss >> std::get<1>(params);
                        break;
                    }
                }
            }

            m_modelOCV = cv::bgsegm::createBackgroundSubtractorGMG(std::get<0>(params), std::get<1>(params));
            break;
        }

        case ALG_CNT:
        {
#if (((CV_VERSION_MAJOR == 3) && (CV_VERSION_MINOR >= 2)) || (CV_VERSION_MAJOR > 3))
            std::vector<std::string> paramsConf = { "minPixelStability", "useHistory", "maxPixelStability", "isParallel" };
            auto params = std::make_tuple(15, 1, 15 * 60, 1);

            for (size_t i = 0; i < paramsConf.size(); ++i)
            {
                auto conf = config.find(paramsConf[i]);
                if (conf != config.end())
                {
                    std::stringstream ss(conf->second);

                    switch (i)
                    {
                    case 0:
                        ss >> std::get<0>(params);
                        break;
                    case 1:
                        ss >> std::get<1>(params);
                        break;
                    case 2:
                        ss >> std::get<2>(params);
                        break;
                    case 3:
                        ss >> std::get<3>(params);
                        break;
                    }
                }
            }

            m_modelOCV = cv::bgsegm::createBackgroundSubtractorCNT(std::get<0>(params), std::get<1>(params) != 0, std::get<2>(params), std::get<3>(params) != 0);
#else
            std::cerr << "OpenCV CNT algorithm is not implemented! Used Vibe by default." << std::endl;
            failed = true;
#endif
            break;
        }

#else
        case ALG_MOG:
        case ALG_GMG:
        case ALG_CNT:
            std::cerr << "OpenCV bgfg algorithms are not implemented! Used Vibe by default." << std::endl;
            failed = true;
            break;
#endif

        case ALG_SuBSENSE:
            m_modelSuBSENSE = std::make_unique<BackgroundSubtractorSuBSENSE>(); // default params
            break;

        case ALG_LOBSTER:
            m_modelSuBSENSE = std::make_unique<BackgroundSubtractorLOBSTER>();  // default params
            break;

        case ALG_MOG2:
        {
            std::vector<std::string> paramsConf = { "history", "varThreshold", "detectShadows" };
            auto params = std::make_tuple(500, 16, 1);

            for (size_t i = 0; i < paramsConf.size(); ++i)
            {
                auto conf = config.find(paramsConf[i]);
                if (conf != config.end())
                {
                    std::stringstream ss(conf->second);

                    switch (i)
                    {
                    case 0:
                        ss >> std::get<0>(params);
                        break;
                    case 1:
                        ss >> std::get<1>(params);
                        break;
                    case 2:
                        ss >> std::get<2>(params);
                        break;
                    }
                }
            }

            m_modelOCV = cv::createBackgroundSubtractorMOG2(std::get<0>(params), std::get<1>(params), std::get<2>(params) != 0).dynamicCast<cv::BackgroundSubtractor>();
            break;
        }

        default:
        {
            m_algType = ALG_VIBE;
            failed = false;
            break;
        }
        }
        if (failed)
        {
            m_algType = ALG_VIBE;
            failed = false;
        }
    }
    return !failed;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void BackgroundSubtract::Subtract(const cv::UMat& image, cv::UMat& foreground)
{
    auto GetImg = [&]() -> cv::UMat
    {
            if (image.channels() != m_channels)
    {
            if (image.channels() == 1)
    {
            cv::UMat newImg;
        #if (CV_VERSION_MAJOR < 4)
            cv::cvtColor(image, newImg, CV_GRAY2BGR);
        #else
            cv::cvtColor(image, newImg, cv::COLOR_GRAY2BGR);
        #endif
            return newImg;
}
            else if (image.channels() == 3)
    {
            cv::UMat newImg;
        #if (CV_VERSION_MAJOR < 4)
            cv::cvtColor(image, newImg, CV_BGR2GRAY);
        #else
            cv::cvtColor(image, newImg, cv::COLOR_BGR2GRAY);
        #endif
            return newImg;
}
}
            return image;
};

    switch (m_algType)
    {
    case ALG_VIBE:
        m_modelVibe->update(GetImg().getMat(cv::ACCESS_READ));
        foreground = m_modelVibe->getMask().getUMat(cv::ACCESS_READ);
        break;

    case ALG_MOG:
    case ALG_GMG:
    case ALG_CNT:
#ifdef USE_OCV_BGFG
        m_modelOCV->apply(GetImg(), foreground);
        break;
#else
        std::cerr << "OpenCV bgfg algorithms are not implemented!" << std::endl;
        break;
#endif

    case ALG_SuBSENSE:
    case ALG_LOBSTER:
        if (foreground.size() != image.size())
        {
            m_modelSuBSENSE->initialize(GetImg().getMat(cv::ACCESS_READ), cv::Mat());
            foreground.create(image.size(), CV_8UC1);
        }
        else
        {
            m_modelSuBSENSE->apply(GetImg(), foreground);
        }
        break;

    case ALG_MOG2:
        m_modelOCV->apply(GetImg(), foreground);
        cv::threshold(foreground, foreground, 200, 255, cv::THRESH_BINARY);
        break;

    default:
        m_modelVibe->update(GetImg().getMat(cv::ACCESS_READ));
        foreground = m_modelVibe->getMask().getUMat(cv::ACCESS_READ);
        break;
    }

    //cv::imshow("before", foreground);

    cv::medianBlur(foreground, foreground, 3);

    cv::Mat dilateElement = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3), cv::Point(-1, -1));
    cv::dilate(foreground, foreground, dilateElement, cv::Point(-1, -1), 2);

    //cv::imshow("after", foreground);
}
