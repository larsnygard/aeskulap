// Headless test: SeriesMath (Sub, MIP) engine + DICOM SC writer round-trip.
#include "seriesmath.h"
#include "poolinstance.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

static int failures = 0;
#define CHECK(cond, name) do { \
	if(cond) { printf("PASS %s\n", name); } \
	else { printf("FAIL %s\n", name); failures++; } \
} while(0)

static Glib::RefPtr<ImagePool::Instance> make_instance(int w, int h, int frameval, int sign_adj = 0)
{
	// Build an in-memory 16-bit unsigned SC dataset and run it through
	// ImagePool::Instance::create (the real production decode path).
	DcmFileFormat ff;
	DcmDataset* dset = ff.getDataset();
	dset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
	char uid[64];
	dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT);
	dset->putAndInsertString(DCM_SOPInstanceUID, uid);
	dset->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5.10");
	dset->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.10.1");
	dset->putAndInsertString(DCM_Modality, "SC");
	dset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
	dset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
	dset->putAndInsertUint16(DCM_PixelRepresentation, 0);
	dset->putAndInsertUint16(DCM_Rows, (Uint16)h);
	dset->putAndInsertUint16(DCM_Columns, (Uint16)w);
	dset->putAndInsertUint16(DCM_BitsAllocated, 16);
	dset->putAndInsertUint16(DCM_BitsStored, 16);
	dset->putAndInsertUint16(DCM_HighBit, 15);
	std::vector<Uint16> px((size_t)w * h, (Uint16)(frameval + sign_adj));
	dset->putAndInsertUint16Array(DCM_PixelData, &px[0], (Uint32)(px.size() * 2));
	dset->putAndInsertSint16(DCM_RescaleIntercept, 0);
	dset->putAndInsertSint16(DCM_RescaleSlope, 1);
	return ImagePool::Instance::create(dset);
}

static int read_first_pixel(const std::string& path, int* w, int* h)
{
	*w = *h = 0;
	DcmFileFormat ff;
	if(ff.loadFile(path.c_str()).bad()) {
		return -99999;
	}
	DcmDataset* dset = ff.getDataset();
	Uint16 rows = 0, cols = 0;
	dset->findAndGetUint16(DCM_Rows, rows);
	dset->findAndGetUint16(DCM_Columns, cols);
	*w = cols;
	*h = rows;
	unsigned long n = 0;
	const Uint16* px = NULL;
	if(dset->findAndGetUint16Array(DCM_PixelData, px, &n).bad() || n < 1) {
		return -99998;
	}
	return (int)(gint16)px[0];
}

int main()
{
	printf("== SeriesMath tests ==\n");

	// ---- load_pixels applies modality LUT ----
	{
		Glib::RefPtr<ImagePool::Instance> inst = make_instance(4, 4, 50);
		CHECK((bool)inst, "instance created");
		std::vector<gint32> px;
		int w = 0, h = 0;
		bool sgn = false;
		CHECK(Aeskulap::SeriesMath::load_pixels(inst, px, w, h, sgn), "load_pixels ok");
		CHECK(w == 4 && h == 4 && px.size() == 16, "load_pixels dims 4x4");
		CHECK(px[0] == 50 && px[15] == 50, "load_pixels values = 50");
		CHECK(!sgn, "load_pixels unsigned");
	}

	// ---- MIP: frames 10, 20, 30 -> 30 everywhere ----
	std::string mipdir = Glib::build_filename(Glib::get_tmp_dir(), "aesk-mip-test"); g_mkdir_with_parents(mipdir.c_str(), 0755);
	{
		std::vector<Glib::RefPtr<ImagePool::Instance> > a;
		a.push_back(make_instance(4, 4, 10));
		a.push_back(make_instance(4, 4, 20));
		a.push_back(make_instance(4, 4, 30));
		Aeskulap::SeriesMath::Result r = Aeskulap::SeriesMath::maximum_intensity_project(
			a, mipdir, "1.2.3.4.5.10", "1.2.3.4.5.10.99", "test MIP");
		CHECK(r.frames == 1 && r.width == 4 && r.height == 4, "MIP 1 frame 4x4");
		int w = 0, h = 0;
		std::string p = mipdir + "/mip0001.dcm";
		int v = read_first_pixel(p, &w, &h);
		CHECK(w == 4 && h == 4, "MIP file dims 4x4");
		CHECK(v == 30, "MIP pixel = max(10,20,30) = 30");
	}

	// ---- Sub: 50 - 30 = 20 (and 30-50 = -20, signed domain) ----
	std::string subdir = Glib::build_filename(Glib::get_tmp_dir(), "aesk-sub-test"); g_mkdir_with_parents(subdir.c_str(), 0755);
	{
		std::vector<Glib::RefPtr<ImagePool::Instance> > a, b;
		a.push_back(make_instance(4, 4, 50));
		b.push_back(make_instance(4, 4, 30));
		Aeskulap::SeriesMath::Result r = Aeskulap::SeriesMath::subtract(
			a, b, subdir, "1.2.3.4.5.10", "1.2.3.4.5.10.98", "test SUB");
		CHECK(r.frames == 1, "SUB 1 frame");
		int w = 0, h = 0;
		std::string p = subdir + "/sub0001.dcm";
		int v = read_first_pixel(p, &w, &h);
		CHECK(v == 20, "SUB pixel = 50-30 = 20");

		// reversed -> -20; signed pixel representation keeps negative
		Aeskulap::SeriesMath::Result r2 = Aeskulap::SeriesMath::subtract(
			b, a, subdir, "1.2.3.4.5.10", "1.2.3.4.5.10.97", "test SUB neg");
		int v2 = read_first_pixel(std::string(subdir) + "/sub0001.dcm", &w, &h);
		CHECK(r2.frames == 1 && v2 == -20, "SUB reversed = -20 (signed)");

		// grid mismatch -> no output
		std::vector<Glib::RefPtr<ImagePool::Instance> > c;
		c.push_back(make_instance(8, 8, 30));
		Aeskulap::SeriesMath::Result r3 = Aeskulap::SeriesMath::subtract(
			a, c, subdir, "1.2.3.4.5.10", "1.2.3.4.5.10.96", "mismatch");
		CHECK(r3.frames == 0, "SUB grid mismatch -> 0 frames");
	}

	printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
	return failures ? 1 : 0;
}
