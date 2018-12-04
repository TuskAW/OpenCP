#include <opencp.hpp>
#include "test.hpp"
using namespace std;

int main(int argc, char** argv)
{
	
	//guiConsoleTest();
	
	Mat left = imread("img/stereo/Dolls/view1.png");
	Mat right = imread("img/stereo/Dolls/view5.png");
	Mat dmap = imread("img/stereo/Dolls/disp1.png", 0);
	guiDissolveSlide(left, dmap);

	Mat img = imread("img/lenna.png");
	Mat a;
	resize(img, a, Size(513, 513));
	splitmergeTest(a); return 0;
	//Mat img = imread("img/Kodak/kodim07.png");
	//Mat img = imread("img/b.png");
	
	//guiUpsampleTest(img);return 0;
	//guiDomainTransformFilterTest(img);
	//guiMedianFilterTest(img);
	//VisualizeDenormalKernel vdk;
	//vdk.run(img);
	//return 0;
	//VizKernel vk;
	//vk.run(img, 2);
	//HazeRemove2 hz;
	//Mat haze = imread("img/haze/swans.png");
	//Mat haze = imread("img/haze/canyon.png");
	
	//hz.gui(haze, "haze");
	//guiStereoSGBMTest(left, right, 96);

	//iirGuidedFilterTest2(img); return 0;
	//iirGuidedFilterTest1(dmap, left); return 0;
	//iirGuidedFilterTest(); return 0;
	//iirGuidedFilterTest(left); return 0;
	//fitPlaneTest(); return 0;
	//guiWeightMapTest(); return 0;
	//guiStereo(); return 0;
	//guiPlotTest(); return 0;
	//zoom(argc, argv);return 0;

	//guiGeightedJointBilateralFilterTest();
	//Mat haze = imread("img/haze/haze2.jpg"); guiHazeRemoveTest(haze);
	//Mat fuji = imread("img/fuji.png"); guiDenoiseTest(fuji);
	//Mat ff3 = imread("img/pixelart/ff3.png");

	Mat src = imread("img/lenna.png",0);
	//Mat src = imread("img/Kodak/kodim07.png",0);
	guiIterativeBackProjectionTest(src);
	//Mat src = imread("img/Kodak/kodim15.png",0);
	
	//Mat src = imread("img/cave-flash.png");
	//Mat src = imread("img/feathering/toy.png");
	//Mat src = imread("Clipboard01.png");

	//timeGuidedFilterTest(src);
	//Mat src = imread("img/flower.png");
	//Mat src = imread("img/teddy_disp1.png");
	//Mat src_ = imread("img/stereo/Art/view1.png",0);
	//	Mat src;
	//	copyMakeBorder(src_,src,0,1,0,1,BORDER_REPLICATE);

	//Mat src = imread("img/lenna.png", 0);


	


	//Mat src = imread("img/stereo/Dolls/view1.png");
	//guiDenoiseTest(src);
	guiBilateralFilterTest(src);
	Mat ref = imread("img/stereo/Dolls/view6.png");
	//guiColorCorrectionTest(src, ref); return 0;
	//Mat src = imread("img/flower.png");
	//guiAnalysisImage(src);
	Mat dst = src.clone();
	//paralleldenoise(src, dst, 5);
	Mat disp = imread("img/stereo/Dolls/disp1.png",0 );
	//	Mat src;
	Mat dest;


	//guiCrossBasedLocalFilter(src); return 0;
	//guiHistgramTest(src);
	//Mat src = imread("img/kodim22.png");
	//Mat src = imread("img/teddy_view1.png");

	//eraseBoundary(src,10);
	Mat mega;
	resize(src, mega, Size(1024, 1024));
	//	resize(src,mega,Size(1024,1024));
	//resize(src,mega,Size(640,480));

	//guiDualBilateralFilterTest(src,disp);
	//guiGausianFilterTest(src); return 0;

	//guiCoherenceEnhancingShockFilter(src, dest);
	
	Mat gray;
	cvtColor(src, gray, COLOR_BGR2GRAY);
	//guiDisparityPlaneFitSLICTest(src, ref, disp); return 0;
	getPSNRRealtimeO1BilateralFilterKodak();
	guiRealtimeO1BilateralFilterTest(src); return 0;
	
	Mat flashImg = imread("img/flash/cave-flash.png");
	Mat noflashImg = imread("img/flash/cave-noflash.png");
	Mat noflashImgGray; cvtColor(noflashImg, noflashImgGray, COLOR_BGR2GRAY);
	Mat flashImgGray; cvtColor(flashImg, flashImgGray, COLOR_BGR2GRAY);
	Mat fmega, nmega;
	resize(flashImgGray, fmega, Size(1024, 1024));
	resize(noflashImg, nmega, Size(1024, 1024));
	
	guiSLICTest(src);
	//guiEdgePresevingFilterOpenCV(src);

	//guiJointRealtimeO1BilateralFilterTest(noflashImgGray, flashImgGray); return 0;
	//guiJointRealtimeO1BilateralFilterTest(noflashImg, flashImgGray); return 0;
	//guiJointRealtimeO1BilateralFilterTest(noflashImgGray, flashImg); return 0;
	//guiJointRealtimeO1BilateralFilterTest(noflashImg, flashImg); return 0;

	//guiWeightedHistogramFilterTest(noflashImgGray, flashImg); return 0;
	//guiRealtimeO1BilateralFilterTest(noflashImgGray); return 0;
	//guiRealtimeO1BilateralFilterTest(src); return 0;
	//guiDMFTest(nmega, nmega, fmega); return 0;
	//guiGausianFilterTest(src); return 0;


	//guiAlphaBlend(ff3,ff3);
	//guiJointNearestFilterTest(ff3);
	//guiViewSynthesis();
	
	//guiSeparableBilateralFilterTest(src);
	//guiBilateralFilterSPTest(mega);
	//guiRecursiveBilateralFilterTest(mega);
	//fftTest(src);

	Mat feather = imread("img/feathering/toy-mask.png");
	//Mat guide = imread("img/feathering/toy.png");
	//timeBirateralTest(mega);

	Mat flash = imread("img/cave-flash.png");
	Mat noflash = imread("img/cave-noflash.png");
	Mat disparity = imread("img/teddy_disp1.png", 0);
	//guiJointBirateralFilterTest(noflash,flash);
	//guiBinalyWeightedRangeFilterTest(disparity);
	//guiCodingDistortionRemoveTest(disparity);
	//guiJointBinalyWeightedRangeFilterTest(noflash,flash);

	//guiNonLocalMeansTest(src);

	

	//application 
	//guiDetailEnhancement(src);
	//guiGuidedFilterTest(mega);
	//	guiDomainTransformFilterTest(mega);
	return 0;
}