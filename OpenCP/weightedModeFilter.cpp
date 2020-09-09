#include "weightedModeFilter.hpp"
#include "debugcp.hpp"
#include <inlineSIMDFunctions.hpp>
using namespace std;
using namespace cv;

/*
src：入力画像
guide:guide画像（srcと同じでもOK）
dst:出力画像
r:カーネル半径
truncate: ヒストグラムを加算するときの半径．±truncate分加算．加算の仕方はmetricで指定
sigmaColor：カラーのガウシアンのパラメータ
sigmaSpace：空間のガウシアンのパラメータ
metric:
L0_NORM:何もしない．今の実装はたぶんこれ
L1_NORM:L1ノルムでヒストグラムを加算
L1_NORM:L2ノルムでヒストグラムを加算
EXP:ガウシアンでヒストグラムを加算

method:
Histogram::BILATERAL　バイラテラルの加算：現在の実装はたぶんこれ
Histogram::GAUSSIAN　ガウシアンフィルタで加算
Histogram::NO_WEIGHT　ボックスフィルタで加算
*/
namespace cp
{
#define sqr(a) ((a)*(a))

	WeightedHistogram::~WeightedHistogram()
	{
		_mm_free(histbuff);
	}

	WeightedHistogram::WeightedHistogram(int truncate_val, int mode_, const int max_val)
	{
		histSize = max_val+1;
		histbuffsize = get_simd_ceil(max_val+1 + truncate_val * 2, 8);
		histbuff = (float*)_mm_malloc(sizeof(float) * histbuffsize, 16);
		hist = histbuff + truncate_val;//border +-truncate_val
		truncate = truncate_val;
		mode = mode_;

		clear();
	}

	int WeightedHistogram::returnVal()
	{
		if (mode == 0)
			return returnMax();
		else
			return returnMedian();
	}

	void WeightedHistogram::clear()
	{
		histMin = 0;
		histMax = histSize;
		const int simd_size = histbuffsize / 8;
		float* h = histbuff;
		for (int i = 0; i < simd_size; i++)
		{
			_mm256_store_ps(h, _mm256_setzero_ps());
			h += 8;
		}
	}

	void WeightedHistogram::addWithRange(float addval, int bin, int metric)
	{
		histMax = max(histMax, bin + truncate);
		histMin = min(histMin, bin - truncate);
		hist[bin] += addval;

		if (metric != L0_NORM)
		{
			float val;
			if (metric == L1_NORM)
			{
				for (int i = 1; i < truncate; i++)
				{
					val = addval * ((float)(truncate - i) / (float)truncate);
					hist[bin + i] += val;
					hist[bin - i] += val;
				}
			}
			else if (metric == L2_NORM)
			{
				float div = 1.f / (float)(sqr(truncate));
				for (int i = 1; i < truncate; i++)
				{
					val = addval * ((float)sqr((truncate - i)) * div);
					hist[bin + i] += val;
					hist[bin - i] += val;
				}
			}
			else if (metric == EXP)
			{
				for (int i = 1; i < truncate; i++)
				{
					val = addval * (1 - exp(-((float)sqr(i) / (float)(2 * sqr(truncate)))));
					hist[bin + i] += val;
					hist[bin - i] += val;
				}
			}
		}
	}

	void WeightedHistogram::add(float addval, int bin, int metric)
	{
		hist[bin] += addval;

		if (metric != L0_NORM)
		{
			float val;
			if (metric == L1_NORM)
			{
				for (int i = 1; i < truncate; i++)
				{
					val = addval * ((float)(truncate - i) / (float)truncate);
					hist[bin + i] += val;
					hist[bin - i] += val;
				}
			}
			else if (metric == L2_NORM)
			{
				float div = 1.f / (float)(sqr(truncate));
				for (int i = 1; i < truncate; i++)
				{
					val = addval * ((float)sqr((truncate - i)) * div);
					hist[bin + i] += val;
					hist[bin - i] += val;
				}
			}
			else if (metric == EXP)
			{
				for (int i = 1; i < truncate; i++)
				{
					val = addval * (1 - exp(-((float)sqr(i) / (float)(2 * sqr(truncate)))));
					hist[bin + i] += val;
					hist[bin - i] += val;
				}
			}
		}
	}

	int WeightedHistogram::returnMax()
	{
		float maxv = 0.f;
		int maxbin;

		for (int i = histMin; i < histMax; i++)
		{
			if (hist[i] > maxv)
			{
				maxv = hist[i];
				maxbin = i;
			}
		}
		return maxbin;
	}

	int WeightedHistogram::returnMedian()
	{
		float maxval = 0.f;
		for (int i = histMin; i < histMax; i++)
		{
			maxval += hist[i];
		}
		const float half_max = maxval * 0.5f;

		int maxbin;
		maxval = 0.f;
		for (int i = histMin; i < histMax; i++)
		{
			maxval += hist[i];
			if (maxval > half_max)
			{
				maxbin = i;
				break;
			}
		}
		return maxbin;
	}

	int WeightedHistogram::returnMaxwithRange()
	{
		histMin = max(histMin, 0);
		histMax = min(histMax, 255);
		float maxv = 0.f;
		int maxbin;

		for (int i = histMin; i < histMax; i++)
		{
			if (hist[i] > maxv)
			{
				maxv = hist[i];
				maxbin = i;
			}
		}
		return maxbin;
	}

	int WeightedHistogram::returnMedianwithRange()
	{
		histMin = max(histMin, 0);
		histMax = min(histMax, 255);
		float maxval = 0.f;
		for (int i = histMin; i < histMax; i++)
		{
			maxval += hist[i];
		}
		const float half_max = maxval * 0.5f;

		int maxbin;
		maxval = 0.f;
		for (int i = histMin; i < histMax; i++)
		{
			maxval += hist[i];
			if (maxval > half_max)
			{
				maxbin = i;
				break;
			}
		}
		return maxbin;
	}

	void bgrSplit(Mat& src, Mat& b, Mat& g, Mat& r)
	{
		vector<Mat> div;

		split(src, div);
		vector<Mat>::const_iterator it = div.begin();
		Mat btemp, gtemp, rtemp;
		btemp = *it++;
		gtemp = *it++;
		rtemp = *it;
		b = btemp.clone();
		g = gtemp.clone();
		r = rtemp.clone();
	}

	void bgrMerge(Mat& dst, Mat& b, Mat& g, Mat& r, int rtype)
	{
		vector<Mat> merge;

		merge.push_back(b);
		merge.push_back(g);
		merge.push_back(r);
		Mat mixed(b.size(), rtype);
		int fromTo[] = { 0, 0, 1, 1, 2, 2 };

		vector<Mat> mix;
		mix.push_back(mixed);

		mixChannels(merge, mix, fromTo, 3);

		dst = mixed.clone();
	}

	void weightedweightedHistogramFilter(Mat& src, Mat& weightMap, Mat& guide, Mat& dst, int r, int truncate, double sig_c1, double sig_c2, double sig_s, int metric, int method)
	{
		int width = src.cols;
		int height = src.rows;

		Mat src2; copyMakeBorder(src, src2, r, r, r, r, 1);

		int mode = WeightedHistogram::MAX;
		if (method >= WeightedHistogram::NO_WEIGHT_MEDIAN)
		{
			mode = WeightedHistogram::MEDIAN;
			method -= WeightedHistogram::NO_WEIGHT_MEDIAN;
		}

		int borderType = cv::BORDER_REPLICATE;
		Mat wmap;
		if (weightMap.depth() == CV_32F)
		{
			copyMakeBorder(weightMap, wmap, r, r, r, r, borderType);
		}
		else
		{
			Mat temp; weightMap.convertTo(temp, CV_32F);
			copyMakeBorder(temp, wmap, r, r, r, r, borderType);
		}

		if (guide.channels() == 3)
		{
			Mat B, G, R;
			bgrSplit(guide, B, G, R);

			Mat guideB; copyMakeBorder(B, guideB, r, r, r, r, borderType);
			Mat guideG; copyMakeBorder(G, guideG, r, r, r, r, borderType);
			Mat guideR; copyMakeBorder(R, guideR, r, r, r, r, borderType);

			float* lutc1 = new float[768];
			float* lutc2 = new float[768];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256 * 3; i++)
			{
				lutc1[i] = (float)(exp(-sqr(i) / (2 * sqr(sig_c1))));
				lutc2[i] = (float)(exp(-sqr(i) / (2 * sqr(sig_c2))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						const uchar ss = src.at<uchar>(y, x);
						const uchar bb = B.at<uchar>(y, x);
						const uchar gg = G.at<uchar>(y, x);
						const uchar rr = R.at<uchar>(y, x);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* bp = guideB.ptr<uchar>(y + j); bp += x;
							uchar* gp = guideG.ptr<uchar>(y + j); gp += x;
							uchar* rp = guideR.ptr<uchar>(y + j); rp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;

								int diff1 = abs(ss - *sp);
								int diff = abs(bb - *bp) + abs(gg - *gp) + abs(rr - *rp);
								addval *= lutc1[diff] * lutc2[diff];
								h.add(addval, *sp, metric);

								w++;
								sp++;
								bp++;
								gp++;
								rp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
					{
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);
								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc1;
			delete[] lutc2;
			delete[] luts;
		}
		else if (guide.channels() == 1)
		{
			Mat G; copyMakeBorder(guide, G, r, r, r, r, borderType);

			float* lutc1 = new float[256];
			float* lutc2 = new float[256];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256; i++)
			{
				lutc1[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c1))));
				lutc2[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c2))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						const uchar gg = guide.at<uchar>(y, x);
						const uchar ss = src.at<uchar>(y, x);
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* gp = G.ptr<uchar>(y + j); gp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								int diff1 = abs(ss - *sp);
								int diff = abs(gg - *gp);
								addval *= lutc1[diff1] * lutc2[diff];

								h.add(addval, *sp, metric);

								w++;
								sp++;
								gp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
					{
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);

								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc1;
			delete[] lutc2;
			delete[] luts;
		}
	}

	void weightedweightedHistogramFilter(Mat& src, Mat& weightMap, Mat& guide, Mat& dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		int width = src.cols;
		int height = src.rows;

		Mat src2; copyMakeBorder(src, src2, r, r, r, r, 1);

		int mode = WeightedHistogram::MAX;
		if (method >= WeightedHistogram::NO_WEIGHT_MEDIAN)
		{
			mode = WeightedHistogram::MEDIAN;
			method -= WeightedHistogram::NO_WEIGHT_MEDIAN;
		}

		int borderType = cv::BORDER_REPLICATE;
		Mat wmap;
		if (weightMap.depth() == CV_32F)
		{
			copyMakeBorder(weightMap, wmap, r, r, r, r, borderType);
		}
		else
		{
			Mat temp; weightMap.convertTo(temp, CV_32F);
			copyMakeBorder(temp, wmap, r, r, r, r, borderType);
		}

		if (guide.channels() == 3)
		{
			Mat B, G, R;
			bgrSplit(guide, B, G, R);

			Mat guideB; copyMakeBorder(B, guideB, r, r, r, r, borderType);
			Mat guideG; copyMakeBorder(G, guideG, r, r, r, r, borderType);
			Mat guideR; copyMakeBorder(R, guideR, r, r, r, r, borderType);

			float* lutc = new float[768];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256 * 3; i++)
			{
				lutc[i] = (float)(exp(-sqr(i) / (2 * sqr(sig_c))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						const uchar bb = B.at<uchar>(y, x);
						const uchar gg = G.at<uchar>(y, x);
						const uchar rr = R.at<uchar>(y, x);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* bp = guideB.ptr<uchar>(y + j); bp += x;
							uchar* gp = guideG.ptr<uchar>(y + j); gp += x;
							uchar* rp = guideR.ptr<uchar>(y + j); rp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;

								int diff = abs(bb - *bp) + abs(gg - *gp) + abs(rr - *rp);
								addval *= lutc[diff];
								h.add(addval, *sp, metric);

								w++;
								sp++;
								bp++;
								gp++;
								rp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
					{
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);
								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc;
			delete[] luts;
		}
		else if (guide.channels() == 1)
		{
			Mat G; copyMakeBorder(guide, G, r, r, r, r, borderType);

			float* lutc = new float[256];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256; i++)
			{
				lutc[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						const uchar gg = guide.at<uchar>(y, x);
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* gp = G.ptr<uchar>(y + j); gp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								int diff = abs(gg - *gp);
								addval *= lutc[diff];
								h.add(addval, *sp, metric);

								w++;
								sp++;
								gp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
					{
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);

								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc;
			delete[] luts;
		}
	}

	template<typename srcType, typename guideType>
	void weightedHistogramFilter_(Mat& src, Mat& guide, Mat& dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		double src_max;
		minMaxLoc(src, NULL, &src_max);
		src_max = get_simd_ceil(src_max, 4);
		int width = src.cols;
		int height = src.rows;

		Mat srcBorder; copyMakeBorder(src, srcBorder, r, r, r, r, 1);

		int mode = WeightedHistogram::MAX;
		if (method >= WeightedHistogram::NO_WEIGHT_MEDIAN)
		{
			mode = WeightedHistogram::MEDIAN;
			method -= WeightedHistogram::NO_WEIGHT_MEDIAN;
		}

		int borderType = cv::BORDER_REPLICATE;

		if (guide.channels() == 3)
		{
			Mat B, G, R;
			bgrSplit(guide, B, G, R);

			Mat guideB; copyMakeBorder(B, guideB, r, r, r, r, borderType);
			Mat guideG; copyMakeBorder(G, guideG, r, r, r, r, borderType);
			Mat guideR; copyMakeBorder(R, guideR, r, r, r, r, borderType);

			float* lutc = new float[443];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];

			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 443; i++)
			{
				lutc[i] = (float)(exp(-sqr(i) / (2.f * sqr(sig_c))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode, src_max);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						const guideType bb = B.at<guideType>(y, x);
						const guideType gg = G.at<guideType>(y, x);
						const guideType rr = R.at<guideType>(y, x);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							srcType* sp = srcBorder.ptr<srcType>(y + j); sp += x;
							guideType* bp = guideB.ptr<guideType>(y + j); bp += x;
							guideType* gp = guideG.ptr<guideType>(y + j); gp += x;
							guideType* rp = guideR.ptr<guideType>(y + j); rp += x;

							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx];

								int diff = sqrt((bb - *bp) * (bb - *bp) + (gg - *gp) * (gg - *gp) + (rr - *rp) * (rr - *rp));
								addval *= lutc[diff];
								h.add(addval, *sp, metric);

								sp++;
								bp++;
								gp++;
								rp++;
							}
						}
						dst.at<srcType>(y, x) = h.returnVal();
					}
				}
			}
			if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							srcType* sp = srcBorder.ptr<srcType>(y + j); sp += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx];
								h.add(addval, *sp, metric);
								sp++;
							}
						}
						dst.at<srcType>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
					{
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = 1.f;
								h.add(addval, srcBorder.at<srcType>(y + j, x + i), metric);
							}
						}
						dst.at<srcType>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc;
			delete[] luts;
		}
		else if (guide.channels() == 1)
		{

			Mat G; copyMakeBorder(guide, G, r, r, r, r, borderType);

			float* lutc = new float[256];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256; i++)
			{
				lutc[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						const guideType gg = guide.at<guideType>(y, x);
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							srcType* sp = srcBorder.ptr<srcType>(y + j); sp += x;
							guideType* gp = G.ptr<guideType>(y + j); gp += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx];
								int diff = abs(gg - *gp);
								addval *= lutc[diff];
								h.add(addval, *sp, metric);
								sp++;
								gp++;
							}
						}
						dst.at<srcType>(y, x) = h.returnVal();
					}
				}
			}
			if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					for (int x = 0; x < width; x++)
					{
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							srcType* sp = srcBorder.ptr<srcType>(y + j); sp += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx];
								h.add(addval, *sp, metric);
								sp++;
							}
						}
						dst.at<srcType>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
					{
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = 1.f;
								h.add(addval, srcBorder.at<srcType >(y + j, x + i), metric);
							}
						}
						dst.at<srcType>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc;
			delete[] luts;
		}
	}

	void weightedHistogramFilter(Mat& src, Mat& guide, Mat& dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		CV_Assert(guide.depth() == CV_8U);
		if (src.depth() == CV_8U && guide.depth() == CV_8U)weightedHistogramFilter_<uchar, uchar>(src, guide, dst, r, truncate, sig_c, sig_s, metric, method);
		if (src.depth() == CV_16S && guide.depth() == CV_8U)weightedHistogramFilter_<short, uchar>(src, guide, dst, r, truncate, sig_c, sig_s, metric, method);
		if (src.depth() == CV_16U && guide.depth() == CV_8U)weightedHistogramFilter_<ushort, uchar>(src, guide, dst, r, truncate, sig_c, sig_s, metric, method);
		if (src.depth() == CV_32F && guide.depth() == CV_8U)weightedHistogramFilter_<float, uchar>(src, guide, dst, r, truncate, sig_c, sig_s, metric, method);
	}

	//with mask
#include<omp.h>
	void weightedweightedHistogramFilter(Mat& src, Mat& weightMap, Mat& guide, Mat& mask, Mat& dst, int r, int truncate, double sig_c1, double sig_c2, double sig_s, int metric, int method)
	{
		src.copyTo(dst);
		int width = src.cols;
		int height = src.rows;

		Mat src2; copyMakeBorder(src, src2, r, r, r, r, 1);

		int mode = WeightedHistogram::MAX;
		if (method >= WeightedHistogram::NO_WEIGHT_MEDIAN)
		{
			mode = WeightedHistogram::MEDIAN;
			method -= WeightedHistogram::NO_WEIGHT_MEDIAN;
		}

		int borderType = cv::BORDER_REPLICATE;
		Mat wmap;
		if (weightMap.depth() == CV_32F)
		{
			copyMakeBorder(weightMap, wmap, r, r, r, r, borderType);
		}
		else
		{
			Mat temp; weightMap.convertTo(temp, CV_32F);
			copyMakeBorder(temp, wmap, r, r, r, r, borderType);
		}

		if (guide.channels() == 3)
		{
			Mat B, G, R;
			bgrSplit(guide, B, G, R);

			Mat guideB; copyMakeBorder(B, guideB, r, r, r, r, borderType);
			Mat guideG; copyMakeBorder(G, guideG, r, r, r, r, borderType);
			Mat guideR; copyMakeBorder(R, guideR, r, r, r, r, borderType);

			float* lutc1 = new float[768];
			float* lutc2 = new float[768];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256 * 3; i++)
			{
				lutc1[i] = (float)(exp(-sqr(i) / (2 * sqr(sig_c1))));
				lutc2[i] = (float)(exp(-sqr(i) / (2 * sqr(sig_c2))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for schedule(dynamic,1)
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						const uchar ss = src.at<uchar>(y, x);
						const uchar bb = B.at<uchar>(y, x);
						const uchar gg = G.at<uchar>(y, x);
						const uchar rr = R.at<uchar>(y, x);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* bp = guideB.ptr<uchar>(y + j); bp += x;
							uchar* gp = guideG.ptr<uchar>(y + j); gp += x;
							uchar* rp = guideR.ptr<uchar>(y + j); rp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;

								int diff1 = abs(ss - *sp);
								int diff = abs(bb - *bp) + abs(gg - *gp) + abs(rr - *rp);
								addval *= lutc1[diff] * lutc2[diff];
								h.add(addval, *sp, metric);

								w++;
								sp++;
								bp++;
								gp++;
								rp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0) continue;
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);
								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc1;
			delete[] lutc2;
			delete[] luts;
		}
		else if (guide.channels() == 1)
		{
			Mat G; copyMakeBorder(guide, G, r, r, r, r, borderType);

			float* lutc1 = new float[256];
			float* lutc2 = new float[256];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256; i++)
			{
				lutc1[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c1))));
				lutc2[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c2))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						const uchar gg = guide.at<uchar>(y, x);
						const uchar ss = src.at<uchar>(y, x);
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* gp = G.ptr<uchar>(y + j); gp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								int diff1 = abs(ss - *sp);
								int diff = abs(gg - *gp);
								addval *= lutc1[diff1] * lutc2[diff];

								h.add(addval, *sp, metric);

								w++;
								sp++;
								gp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);

								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc1;
			delete[] lutc2;
			delete[] luts;
		}
	}

	void weightedweightedHistogramFilter(Mat& src, Mat& weightMap, Mat& guide, Mat& mask, Mat& dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		src.copyTo(dst);
		int width = src.cols;
		int height = src.rows;

		Mat src2; copyMakeBorder(src, src2, r, r, r, r, 1);

		int mode = WeightedHistogram::MAX;
		if (method >= WeightedHistogram::NO_WEIGHT_MEDIAN)
		{
			mode = WeightedHistogram::MEDIAN;
			method -= WeightedHistogram::NO_WEIGHT_MEDIAN;
		}

		int borderType = cv::BORDER_REPLICATE;
		Mat wmap;
		if (weightMap.depth() == CV_32F)
		{
			copyMakeBorder(weightMap, wmap, r, r, r, r, borderType);
		}
		else
		{
			Mat temp; weightMap.convertTo(temp, CV_32F);
			copyMakeBorder(temp, wmap, r, r, r, r, borderType);
		}

		if (guide.channels() == 3)
		{
			Mat B, G, R;
			bgrSplit(guide, B, G, R);

			Mat guideB; copyMakeBorder(B, guideB, r, r, r, r, borderType);
			Mat guideG; copyMakeBorder(G, guideG, r, r, r, r, borderType);
			Mat guideR; copyMakeBorder(R, guideR, r, r, r, r, borderType);

			float* lutc = new float[768];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256 * 3; i++)
			{
				lutc[i] = (float)(exp(-sqr(i) / (2 * sqr(sig_c))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;

						h.clear();
						const uchar bb = B.at<uchar>(y, x);
						const uchar gg = G.at<uchar>(y, x);
						const uchar rr = R.at<uchar>(y, x);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* bp = guideB.ptr<uchar>(y + j); bp += x;
							uchar* gp = guideG.ptr<uchar>(y + j); gp += x;
							uchar* rp = guideR.ptr<uchar>(y + j); rp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;

								int diff = abs(bb - *bp) + abs(gg - *gp) + abs(rr - *rp);
								addval *= lutc[diff];
								h.add(addval, *sp, metric);

								w++;
								sp++;
								bp++;
								gp++;
								rp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);
								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc;
			delete[] luts;
		}
		else if (guide.channels() == 1)
		{
			Mat G; copyMakeBorder(guide, G, r, r, r, r, borderType);

			float* lutc = new float[256];
			float* luts = new float[(2 * r + 1) * (2 * r + 1)];


			for (int j = 0, idx = 0; j < 2 * r + 1; j++)
			{
				for (int i = 0; i < 2 * r + 1; i++, idx++)
				{
					luts[idx] = (float)(exp(-(sqr(i - r) + sqr(j - r)) / (2 * sqr(sig_s))));
				}
			}
			for (int i = 0; i < 256; i++)
			{
				lutc[i] = (float)(exp(-sqr(i) / (2.0 * sqr(sig_c))));
			}

			if (method == WeightedHistogram::BILATERAL_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						const uchar gg = guide.at<uchar>(y, x);
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							uchar* gp = G.ptr<uchar>(y + j); gp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								int diff = abs(gg - *gp);
								addval *= lutc[diff];
								h.add(addval, *sp, metric);

								w++;
								sp++;
								gp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::GAUSSIAN_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					WeightedHistogram h(truncate, mode);
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						h.clear();
						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							uchar* sp = src2.ptr<uchar>(y + j); sp += x;
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = luts[idx] * *w;
								h.add(addval, *sp, metric);

								w++;
								sp++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			else if (method == WeightedHistogram::NO_WEIGHT_MODE)
			{
#pragma omp parallel for
				for (int y = 0; y < height; y++)
				{
					uchar* msk = mask.ptr<uchar>(y);
					for (int x = 0; x < width; x++)
					{
						if (msk[x] == 0)continue;
						WeightedHistogram h(truncate, mode);

						for (int j = 0, idx = 0; j < 2 * r + 1; j++)
						{
							float* w = wmap.ptr<float>(y + j); w += x;
							for (int i = 0; i < 2 * r + 1; i++, idx++)
							{
								float addval = *w;
								h.add(addval, src2.at<uchar>(y + j, x + i), metric);

								w++;
							}
						}
						dst.at<uchar>(y, x) = h.returnVal();
					}
				}
			}
			delete[] lutc;
			delete[] luts;
		}
	}

	void weightedModeFilter(InputArray src, InputArray guide, OutputArray dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();
		weightedHistogramFilter(s, g, d, r, truncate, sig_c, sig_s, metric, method);
	}

	void weightedMedianFilter(InputArray src, InputArray guide, OutputArray dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();
		weightedHistogramFilter(s, g, d, r, truncate, sig_c, sig_s, metric, method + WeightedHistogram::NO_WEIGHT_MEDIAN);
	}

	void weightedweightedModeFilter(InputArray src, InputArray wmap, InputArray guide, OutputArray dst, int r, int truncate, double sig_c1, double sig_c2, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, d, r, truncate, sig_c1, sig_c2, sig_s, metric, method);
	}

	void weightedweightedMedianFilter(InputArray src, InputArray wmap, InputArray guide, OutputArray dst, int r, int truncate, double sig_c1, double sig_c2, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, d, r, truncate, sig_c1, sig_c2, sig_s, metric, method + WeightedHistogram::NO_WEIGHT_MEDIAN);
	}

	void weightedweightedModeFilter(InputArray src, InputArray wmap, InputArray guide, OutputArray dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, d, r, truncate, sig_c, sig_s, metric, method);
	}

	void weightedweightedMedianFilter(InputArray src, InputArray wmap, InputArray guide, OutputArray dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, d, r, truncate, sig_c, sig_s, metric, method + WeightedHistogram::NO_WEIGHT_MEDIAN);
	}

	void weightedweightedModeFilter(InputArray src, InputArray wmap, InputArray guide, InputArray mask, OutputArray dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		Mat m = mask.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, m, d, r, truncate, sig_c, sig_s, metric, method);
	}

	void weightedweightedMedianFilter(InputArray src, InputArray wmap, InputArray guide, InputArray mask, OutputArray dst, int r, int truncate, double sig_c, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		Mat m = mask.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, m, d, r, truncate, sig_c, sig_s, metric, method + WeightedHistogram::NO_WEIGHT_MEDIAN);
	}

	void weightedweightedModeFilter(InputArray src, InputArray wmap, InputArray guide, InputArray mask, OutputArray dst, int r, int truncate, double sig_c1, double sig_c2, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		Mat m = mask.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, m, d, r, truncate, sig_c1, sig_c2, sig_s, metric, method);
	}

	void weightedweightedMedianFilter(InputArray src, InputArray wmap, InputArray guide, InputArray mask, OutputArray dst, int r, int truncate, double sig_c1, double sig_c2, double sig_s, int metric, int method)
	{
		Mat s = src.getMat();
		Mat g = guide.getMat();
		Mat w = wmap.getMat();
		Mat m = mask.getMat();
		if (dst.empty()) dst.create(src.size(), src.type());
		Mat d = dst.getMat();

		weightedweightedHistogramFilter(s, w, g, m, d, r, truncate, sig_c1, sig_c2, sig_s, metric, method + WeightedHistogram::NO_WEIGHT_MEDIAN);
	}
}