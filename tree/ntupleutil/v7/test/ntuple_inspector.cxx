#include <ROOT/RNTupleInspector.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>

#include <TFile.h>
#include <ROOT/TestSupport.hxx>

#include "gmock/gmock.h"

#include "CustomStructUtil.hxx"
#include "ntupleutil_test.hxx"

using ROOT::ENTupleColumnType;
using ROOT::RField;
using ROOT::RFieldBase;
using ROOT::RNTuple;
using ROOT::RNTupleModel;
using ROOT::RNTupleWriteOptions;
using ROOT::RNTupleWriter;
using ROOT::Experimental::RNTupleInspector;

TEST(RNTupleInspector, CreateFromPointer)
{
   FileRaii fileGuard("test_ntuple_inspector_create_from_pointer.root");
   {
      auto model = RNTupleModel::Create();
      RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath());
   }

   std::unique_ptr<TFile> file(TFile::Open(fileGuard.GetPath().c_str()));
   auto ntuple = std::unique_ptr<RNTuple>(file->Get<RNTuple>("ntuple"));
   auto inspector = RNTupleInspector::Create(*ntuple);
   EXPECT_EQ(inspector->GetDescriptor().GetName(), "ntuple");
}

TEST(RNTupleInspector, CreateFromString)
{
   FileRaii fileGuard("test_ntuple_inspector_create_from_string.root");
   {
      RNTupleWriter::Recreate(RNTupleModel::Create(), "ntuple", fileGuard.GetPath());
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());
   EXPECT_EQ(inspector->GetDescriptor().GetName(), "ntuple");

   EXPECT_THROW(RNTupleInspector::Create("nonexistent", fileGuard.GetPath()), ROOT::RException);
}

TEST(RNTupleInspector, CompressionSettings)
{
   FileRaii fileGuard("test_ntuple_inspector_compression_settings.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt = model->MakeField<std::int32_t>("int");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(207);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      *nFldInt = 42;
      ntuple->Fill();
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(207, *inspector->GetCompressionSettings());
   EXPECT_EQ("LZMA (level 7)", inspector->GetCompressionSettingsAsString());
}

// Relevant for RNTuples created with late model extension, see https://github.com/root-project/root/issues/15661 for
// background.
TEST(RNTupleInspector, UnknownCompression)
{
   FileRaii fileGuard("test_ntuple_inspector_unknown_compression.root");
   std::vector<float> refVec{1., 2., 3.};
   {
      auto model = RNTupleModel::Create();

      *model->MakeField<std::vector<float>>("vecFld") = refVec;

      RNTupleWriteOptions opts;
      opts.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), opts);

      ntuple->Fill();
      ntuple->CommitCluster();

      auto modelUpdater = ntuple->CreateModelUpdater();

      modelUpdater->BeginUpdate();
      *modelUpdater->MakeField<std::vector<float>>("extVecFld") = refVec;
      modelUpdater->CommitUpdate();

      ntuple->Fill();
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());
   EXPECT_EQ(505, *inspector->GetCompressionSettings());
}

TEST(RNTupleInspector, Empty)
{
   FileRaii fileGuard("test_ntuple_inspector_empty.root");
   {
      auto writer = RNTupleWriter::Recreate(RNTupleModel::Create(), "ntuple", fileGuard.GetPath());
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());
   EXPECT_FALSE(inspector->GetCompressionSettings());
   EXPECT_EQ("unknown", inspector->GetCompressionSettingsAsString());
}

TEST(RNTupleInspector, SizeUncompressedSimple)
{
   FileRaii fileGuard("test_ntuple_inspector_size_uncompressed_complex.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt = model->MakeField<std::int32_t>("int");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(0);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int i = 0; i < 5; i++) {
         *nFldInt = 1;
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(sizeof(int32_t) * 5, inspector->GetUncompressedSize());
   EXPECT_EQ(inspector->GetCompressedSize(), inspector->GetUncompressedSize());
}

TEST(RNTupleInspector, SizeUncompressedComplex)
{
   FileRaii fileGuard("test_ntuple_inspector_size_uncompressed_complex.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(0);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      nFldObject->Init1();
      ntuple->Fill();
      nFldObject->Init2();
      ntuple->Fill();
      nFldObject->Init3();
      ntuple->Fill();
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   int nIndexCols = inspector->GetColumnCountByType(ENTupleColumnType::kIndex64);
   int nEntries = inspector->GetDescriptor().GetNEntries();

   EXPECT_EQ(2, nIndexCols);
   EXPECT_EQ(3, nEntries);

   EXPECT_EQ(inspector->GetCompressedSize(), inspector->GetUncompressedSize());
}

TEST(RNTupleInspector, SizeCompressedInt)
{
   FileRaii fileGuard("test_ntuple_inspector_size_compressed_int.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt = model->MakeField<std::int32_t>("int");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int32_t i = 0; i < 500; ++i) {
         *nFldInt = i;
         ntuple->Fill();

         // Store the data in ten clusters to be able to test that the size is correctly computed in this way.
         if (i % 50 == 49)
            ntuple->CommitCluster();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(sizeof(int32_t) * 500, inspector->GetUncompressedSize());
   EXPECT_LT(inspector->GetCompressedSize(), inspector->GetUncompressedSize());
   // Check the target size with a 5% tolerance to account for small fluctuations across different platforms.
   std::uint64_t targetSize = 800;
   EXPECT_NEAR(inspector->GetCompressedSize(), targetSize, targetSize * .05f);
   EXPECT_GT(inspector->GetCompressionFactor(), 1);
}

TEST(RNTupleInspector, SizeCompressedComplex)
{
   FileRaii fileGuard("test_ntuple_inspector_size_compressed_complex.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int i = 0; i < 100; ++i) {
         nFldObject->Init1();
         ntuple->Fill();
         nFldObject->Init2();
         ntuple->Fill();
         nFldObject->Init3();
         ntuple->Fill();

         // Store the data in ten clusters to be able to test that the size is correctly computed in this way.
         if (i % 10 == 9)
            ntuple->CommitCluster();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_LT(inspector->GetCompressedSize(), inspector->GetUncompressedSize());
   // Check the target size with a 5% tolerance to account for small fluctuations across different platforms.
   std::uint64_t targetSize = 3210;
   EXPECT_NEAR(inspector->GetCompressedSize(), targetSize, targetSize * .05f);
   EXPECT_GT(inspector->GetCompressionFactor(), 1);
}

TEST(RNTupleInspector, SizeEmpty)
{
   FileRaii fileGuard("test_ntuple_inspector_size_empty.root");
   {
      auto model = RNTupleModel::Create();
      RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath());
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(0, inspector->GetCompressedSize());
   EXPECT_EQ(0, inspector->GetUncompressedSize());
}

TEST(RNTupleInspector, SizeProjectedFields)
{
   FileRaii fileGuard("test_ntuple_inspector_size_projected_fields.root");
   {
      auto model = RNTupleModel::Create();
      auto muonPt = model->MakeField<std::vector<float>>("muonPt");
      muonPt->emplace_back(1.0);
      muonPt->emplace_back(2.0);

      auto nMuons = RFieldBase::Create("nMuons", "ROOT::RNTupleCardinality<std::uint64_t>").Unwrap();
      model->AddProjectedField(std::move(nMuons), [](const std::string &) { return "muonPt"; });

      auto writer = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath());
      writer->Fill();
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(inspector->GetFieldTreeInspector("muonPt").GetUncompressedSize(), inspector->GetUncompressedSize());
   EXPECT_EQ(inspector->GetFieldTreeInspector("muonPt").GetCompressedSize(), inspector->GetCompressedSize());
}

TEST(RNTupleInspector, ColumnInfoCompressed)
{
   FileRaii fileGuard("test_ntuple_inspector_column_info_compressed.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int i = 0; i < 25; ++i) {
         nFldObject->Init1();
         ntuple->Fill();
         nFldObject->Init2();
         ntuple->Fill();
         nFldObject->Init3();
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   std::uint64_t totalOnDiskSize = 0;

   for (std::size_t i = 0; i < inspector->GetDescriptor().GetNLogicalColumns(); ++i) {
      auto colInfo = inspector->GetColumnInspector(i);
      totalOnDiskSize += colInfo.GetCompressedSize();

      EXPECT_GT(colInfo.GetCompressedSize(), 0);
      EXPECT_GT(colInfo.GetUncompressedSize(), 0);
      EXPECT_LT(colInfo.GetCompressedSize(), colInfo.GetUncompressedSize());
   }

   EXPECT_EQ(totalOnDiskSize, inspector->GetCompressedSize());

   EXPECT_THROW(inspector->GetColumnInspector(42), ROOT::RException);
}

TEST(RNTupleInspector, ColumnInfoUncompressed)
{
   FileRaii fileGuard("test_ntuple_inspector_column_info_uncompressed.root");
   {
      auto model = RNTupleModel::Create();

      auto int32fld = std::make_unique<RField<std::int32_t>>("int32");
      int32fld->SetColumnRepresentatives({{ENTupleColumnType::kInt32}});
      model->AddField(std::move(int32fld));

      auto splitReal64fld = std::make_unique<RField<double>>("splitReal64");
      splitReal64fld->SetColumnRepresentatives({{ENTupleColumnType::kSplitReal64}});
      model->AddField(std::move(splitReal64fld));

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(0);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int i = 0; i < 5; ++i) {
         auto e = ntuple->CreateEntry();
         *e->GetPtr<std::int32_t>("int32") = i;
         *e->GetPtr<double>("splitReal64") = i;
         ntuple->Fill(*e);
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   std::uint64_t colTypeSizes[] = {sizeof(std::int32_t), sizeof(double)};

   for (std::size_t i = 0; i < inspector->GetDescriptor().GetNLogicalColumns(); ++i) {
      auto colInfo = inspector->GetColumnInspector(i);
      EXPECT_EQ(colInfo.GetCompressedSize(), colInfo.GetUncompressedSize());
      EXPECT_EQ(colInfo.GetCompressedSize(), colTypeSizes[i] * 5);
   }
}

TEST(RNTupleInspector, ColumnTypeCount)
{
   FileRaii fileGuard("test_ntuple_inspector_column_type_count.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(2, inspector->GetColumnCountByType(ENTupleColumnType::kSplitIndex64));
   EXPECT_EQ(4, inspector->GetColumnCountByType(ENTupleColumnType::kSplitReal32));
   EXPECT_EQ(3, inspector->GetColumnCountByType(ENTupleColumnType::kSplitInt32));
}

TEST(RNTupleInspector, ColumnsByType)
{
   FileRaii fileGuard("test_ntuple_inspector_columns_by_type.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt1 = model->MakeField<std::int64_t>("int1");
      auto nFldInt2 = model->MakeField<std::int64_t>("int2");
      auto nFldFloat = model->MakeField<float>("float");
      auto nFldFloatVec = model->MakeField<std::vector<float>>("floatVec");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(2U, inspector->GetColumnsByType(ENTupleColumnType::kSplitInt64).size());
   for (const auto colId : inspector->GetColumnsByType(ENTupleColumnType::kSplitInt64)) {
      EXPECT_EQ(ENTupleColumnType::kSplitInt64, inspector->GetColumnInspector(colId).GetType());
   }

   EXPECT_EQ(2U, inspector->GetColumnsByType(ENTupleColumnType::kSplitReal32).size());
   for (const auto colId : inspector->GetColumnsByType(ENTupleColumnType::kSplitReal32)) {
      EXPECT_EQ(ENTupleColumnType::kSplitReal32, inspector->GetColumnInspector(colId).GetType());
   }

   EXPECT_EQ(1U, inspector->GetColumnsByType(ENTupleColumnType::kSplitIndex64).size());
   EXPECT_EQ(1U, inspector->GetColumnsByType(ENTupleColumnType::kSplitIndex64).size());
   for (const auto colId : inspector->GetColumnsByType(ENTupleColumnType::kSplitIndex64)) {
      EXPECT_EQ(ENTupleColumnType::kSplitIndex64, inspector->GetColumnInspector(colId).GetType());
   }

   EXPECT_EQ(0U, inspector->GetColumnsByType(ENTupleColumnType::kSplitReal64).size());
}

TEST(RNTupleInspector, ColumnTypes)
{
   FileRaii fileGuard("test_ntuple_inspector_column_types.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt1 = model->MakeField<std::int64_t>("int1");
      auto nFldInt2 = model->MakeField<std::int64_t>("int2");
      auto nFldFloat = model->MakeField<float>("float");
      auto nFldFloatVec = model->MakeField<std::vector<float>>("floatVec");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());
   auto types = inspector->GetColumnTypes();
   EXPECT_THAT(types, testing::UnorderedElementsAre(ENTupleColumnType::kSplitInt64, ENTupleColumnType::kSplitReal32,
                                                    ENTupleColumnType::kSplitIndex64));
}

TEST(RNTupleInspector, PrintColumnTypeInfo)
{
   FileRaii fileGuard("test_ntuple_inspector_print_column_type_info.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt1 = model->MakeField<std::int64_t>("int1");
      auto nFldInt2 = model->MakeField<std::int64_t>("int2");
      auto nFldFloat = model->MakeField<float>("float");
      auto nFldFloatVec = model->MakeField<std::vector<float>>("floatVec");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (unsigned i = 0; i < 10; ++i) {
         *nFldInt1 = static_cast<std::int64_t>(i);
         *nFldInt2 = static_cast<std::int64_t>(i) * 2;
         *nFldFloat = static_cast<float>(i) * .1f;
         *nFldFloatVec = {static_cast<float>(i), 3.14f, static_cast<float>(i) * *nFldFloat};
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   std::stringstream csvOutput;
   inspector->PrintColumnTypeInfo(ROOT::Experimental::ENTupleInspectorPrintFormat::kCSV, csvOutput);

   std::string line;
   std::getline(csvOutput, line);
   EXPECT_EQ("columnType,count,nElements,compressedSize,uncompressedSize,compressionFactor,nPages", line);

   size_t nLines = 0;
   std::string colTypeStr;
   while (std::getline(csvOutput, line)) {
      ++nLines;
      colTypeStr = line.substr(0, line.find(','));

      if (colTypeStr != "SplitIndex64" && colTypeStr != "SplitInt64" && colTypeStr != "SplitReal32")
         FAIL() << "Unexpected column type: " << colTypeStr;
   }
   EXPECT_EQ(nLines, 3U);

   std::stringstream tableOutput;
   inspector->PrintColumnTypeInfo(ROOT::Experimental::ENTupleInspectorPrintFormat::kTable, tableOutput);

   std::getline(tableOutput, line);
   EXPECT_EQ(
      " column type    | count   | # elements  | compressed bytes | uncompressed bytes | compression ratio | # pages ",
      line);

   std::getline(tableOutput, line);
   EXPECT_EQ(
      "----------------|---------|-------------|------------------|--------------------|-------------------|-------",
      line);

   nLines = 0;
   while (std::getline(tableOutput, line)) {
      ++nLines;
      colTypeStr = line.substr(0, line.find('|'));
      colTypeStr.erase(remove_if(colTypeStr.begin(), colTypeStr.end(), isspace), colTypeStr.end());

      if (colTypeStr != "SplitIndex64" && colTypeStr != "SplitInt64" && colTypeStr != "SplitReal32")
         FAIL() << "Unexpected column type: " << colTypeStr;
   }
   EXPECT_EQ(nLines, 3U);
}

TEST(RNTupleInspector, ColumnTypeInfoHist)
{
   FileRaii fileGuard("test_ntuple_inspector_column_type_info_hist.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt1 = model->MakeField<std::int64_t>("int1");
      auto nFldInt2 = model->MakeField<std::int64_t>("int2");
      auto nFldFloat = model->MakeField<float>("float");
      auto nFldFloatVec = model->MakeField<std::vector<float>>("floatVec");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (unsigned i = 0; i < 10; ++i) {
         *nFldInt1 = static_cast<std::int64_t>(i);
         *nFldInt2 = static_cast<std::int64_t>(i) * 2;
         *nFldFloat = static_cast<float>(i) * .1f;
         *nFldFloatVec = {static_cast<float>(i), 3.14f, static_cast<float>(i) * *nFldFloat};
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   auto countHist = inspector->GetColumnTypeInfoAsHist(ROOT::Experimental::ENTupleInspectorHist::kCount);
   EXPECT_STREQ("colTypeCountHist", countHist->GetName());
   EXPECT_STREQ("Column count by type", countHist->GetTitle());
   EXPECT_EQ(4U, countHist->GetNbinsX());
   EXPECT_EQ(inspector->GetDescriptor().GetNPhysicalColumns(), countHist->Integral());

   auto nElemsHist = inspector->GetColumnTypeInfoAsHist(ROOT::Experimental::ENTupleInspectorHist::kNElems, "elemsHist");
   EXPECT_STREQ("elemsHist", nElemsHist->GetName());
   EXPECT_STREQ("Number of elements by column type", nElemsHist->GetTitle());
   EXPECT_EQ(4U, nElemsHist->GetNbinsX());
   std::uint64_t nTotalElems = 0;
   for (const auto &col : inspector->GetDescriptor().GetColumnIterable()) {
      nTotalElems += inspector->GetDescriptor().GetNElements(col.GetPhysicalId());
   }
   EXPECT_EQ(nTotalElems, nElemsHist->Integral());

   auto compressedSizeHist = inspector->GetColumnTypeInfoAsHist(
      ROOT::Experimental::ENTupleInspectorHist::kCompressedSize, "compressedHist", "Compressed bytes per column type");
   EXPECT_STREQ("compressedHist", compressedSizeHist->GetName());
   EXPECT_STREQ("Compressed bytes per column type", compressedSizeHist->GetTitle());
   EXPECT_EQ(4U, compressedSizeHist->GetNbinsX());
   EXPECT_EQ(inspector->GetCompressedSize(), compressedSizeHist->Integral());

   auto uncompressedSizeHist = inspector->GetColumnTypeInfoAsHist(
      ROOT::Experimental::ENTupleInspectorHist::kUncompressedSize, "", "Uncompressed bytes per column type");
   EXPECT_STREQ("colTypeUncompSizeHist", uncompressedSizeHist->GetName());
   EXPECT_STREQ("Uncompressed bytes per column type", uncompressedSizeHist->GetTitle());
   EXPECT_EQ(4U, uncompressedSizeHist->GetNbinsX());
   EXPECT_EQ(inspector->GetUncompressedSize(), uncompressedSizeHist->Integral());
}

TEST(RNTupleInspector, PageSizeDistribution)
{
   FileRaii fileGuard("test_ntuple_inspector_page_size_distribution.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt = model->MakeField<std::int64_t>("int");
      auto nFldFloat = model->MakeField<float>("float");
      auto nFldFloatVec = model->MakeField<std::vector<float>>("floatVec");
      auto nFldDoubleVec = model->MakeField<std::vector<double>>("doubleVec");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      writeOptions.SetInitialUnzippedPageSize(8);
      writeOptions.SetMaxUnzippedPageSize(64);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (unsigned i = 0; i < 100; ++i) {
         *nFldInt = static_cast<std::int64_t>(i);
         *nFldFloat = static_cast<float>(i) * .1f;
         *nFldFloatVec = {static_cast<float>(i), 3.14f, static_cast<float>(i) * *nFldFloat};
         *nFldDoubleVec = {};
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   int intColId = inspector->GetColumnsByType(ENTupleColumnType::kSplitInt64)[0];
   auto intPageSizeHisto = inspector->GetPageSizeDistribution(intColId);
   EXPECT_STREQ("pageSizeHist", intPageSizeHisto->GetName());
   EXPECT_STREQ(Form("Page size distribution for column with ID %d", intColId), intPageSizeHisto->GetTitle());
   EXPECT_STREQ("Page size (B)", intPageSizeHisto->GetXaxis()->GetTitle());
   EXPECT_STREQ("N_{pages}", intPageSizeHisto->GetYaxis()->GetTitle());
   EXPECT_EQ(64, intPageSizeHisto->GetNbinsX());
   // Make sure that all page sizes are included in the histogram
   int nIntPages = inspector->GetColumnInspector(intColId).GetNPages();
   EXPECT_EQ(nIntPages, intPageSizeHisto->Integral());

   auto floatPageSizeHisto = inspector->GetPageSizeDistribution(ENTupleColumnType::kSplitReal32, "floatPageSize",
                                                                "Float page size distribution", 100);
   EXPECT_STREQ("floatPageSize", floatPageSizeHisto->GetName());
   EXPECT_STREQ("Float page size distribution", floatPageSizeHisto->GetTitle());
   EXPECT_STREQ("Page size (B)", floatPageSizeHisto->GetXaxis()->GetTitle());
   EXPECT_STREQ("N_{pages}", floatPageSizeHisto->GetYaxis()->GetTitle());
   EXPECT_EQ(100, floatPageSizeHisto->GetNbinsX());
   // Make sure that all page sizes are included in the histogram
   int nFloatPages = 0;
   for (const auto colId : inspector->GetColumnsByType(ENTupleColumnType::kSplitReal32)) {
      nFloatPages += inspector->GetColumnInspector(colId).GetNPages();
   }
   EXPECT_EQ(nFloatPages, floatPageSizeHisto->Integral());

   auto multipleColsSizeHisto = inspector->GetPageSizeDistribution({0, 1, 2});
   EXPECT_STREQ("pageSizeHist", multipleColsSizeHisto->GetName());
   EXPECT_STREQ("Page size distribution", multipleColsSizeHisto->GetTitle());
   int nPages = inspector->GetColumnInspector(0).GetNPages() + inspector->GetColumnInspector(1).GetNPages() +
                inspector->GetColumnInspector(2).GetNPages();
   EXPECT_EQ(nPages, multipleColsSizeHisto->Integral());

   auto intFloatPageSizeHisto = inspector->GetPageSizeDistribution(
      {ENTupleColumnType::kSplitInt64, ENTupleColumnType::kSplitReal32}, "intFloatPageSize");
   EXPECT_STREQ("intFloatPageSize", intFloatPageSizeHisto->GetName());
   EXPECT_STREQ("Per-column type page size distribution", intFloatPageSizeHisto->GetTitle());
   EXPECT_EQ(2, intFloatPageSizeHisto->GetNhists());

   int intFloatIntegral = 0;
   for (auto hist : TRangeDynCast<TH1D>(intFloatPageSizeHisto->GetHists())) {
      intFloatIntegral += hist->Integral();
   }
   EXPECT_EQ(nIntPages + nFloatPages, intFloatIntegral);

   auto allColsSizeHisto = inspector->GetPageSizeDistribution();
   nPages = 0;
   for (const auto &col : inspector->GetDescriptor().GetColumnIterable()) {
      nPages += inspector->GetColumnInspector(col.GetPhysicalId()).GetNPages();
   }
   int allColsIntegral = 0;
   for (auto hist : TRangeDynCast<TH1D>(allColsSizeHisto->GetHists())) {
      allColsIntegral += hist->Integral();
   }
   EXPECT_EQ(nPages, allColsIntegral);

   // Requesting a histogram for a column with a physical ID not present in the given RNTuple should throw
   EXPECT_THROW(inspector->GetPageSizeDistribution(inspector->GetDescriptor().GetNPhysicalColumns() + 1),
                ROOT::RException);

   // Requesting a histogram for a column type not present in the given RNTuple should give an empty histogram
   auto nonExistingTypeHisto = inspector->GetPageSizeDistribution(ENTupleColumnType::kReal32);
   EXPECT_EQ(0, nonExistingTypeHisto->Integral());

   // Requesting a histogram for a column type without pages in the given RNTuple should give an empty histogram
   auto emptyTypeHisto = inspector->GetPageSizeDistribution(ENTupleColumnType::kSplitReal64);
   EXPECT_EQ(0, emptyTypeHisto->Integral());

   // Requesting a histogram for a column  without pages in the given RNTuple should give an empty histogram
   auto doubleColumns = inspector->GetColumnsByType(ENTupleColumnType::kSplitReal64);
   ASSERT_EQ(1, doubleColumns.size());
   auto emptyColumnHisto = inspector->GetPageSizeDistribution({doubleColumns[0]});
   EXPECT_EQ(0, emptyColumnHisto->Integral());
}

TEST(RNTupleInspector, FieldInfoCompressed)
{
   FileRaii fileGuard("test_ntuple_inspector_field_info_compressed.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int i = 0; i < 25; ++i) {
         nFldObject->Init1();
         ntuple->Fill();
         nFldObject->Init2();
         ntuple->Fill();
         nFldObject->Init3();
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   auto topFieldInfo = inspector->GetFieldTreeInspector("object");

   EXPECT_GT(topFieldInfo.GetCompressedSize(), 0);
   EXPECT_EQ(topFieldInfo.GetUncompressedSize(), inspector->GetUncompressedSize());
   EXPECT_LT(topFieldInfo.GetCompressedSize(), topFieldInfo.GetUncompressedSize());

   std::uint64_t subFieldOnDiskSize = 0;
   std::uint64_t subFieldInMemorySize = 0;

   for (const auto &subField : inspector->GetDescriptor().GetFieldIterable(topFieldInfo.GetDescriptor().GetId())) {
      auto subFieldInfo = inspector->GetFieldTreeInspector(subField.GetId());
      subFieldOnDiskSize += subFieldInfo.GetCompressedSize();
      subFieldInMemorySize += subFieldInfo.GetUncompressedSize();
   }

   EXPECT_EQ(topFieldInfo.GetCompressedSize(), subFieldOnDiskSize);
   EXPECT_EQ(topFieldInfo.GetUncompressedSize(), subFieldInMemorySize);

   EXPECT_THROW(inspector->GetFieldTreeInspector("invalid_field"), ROOT::RException);
   EXPECT_THROW(inspector->GetFieldTreeInspector(inspector->GetDescriptor().GetNFields()), ROOT::RException);
}

TEST(RNTupleInspector, FieldInfoUncompressed)
{
   FileRaii fileGuard("test_ntuple_inspector_field_info_uncompressed.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(0);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);

      for (int i = 0; i < 25; ++i) {
         nFldObject->Init1();
         ntuple->Fill();
         nFldObject->Init2();
         ntuple->Fill();
         nFldObject->Init3();
         ntuple->Fill();
      }
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   auto topFieldInfo = inspector->GetFieldTreeInspector("object");

   EXPECT_EQ(topFieldInfo.GetCompressedSize(), topFieldInfo.GetUncompressedSize());

   std::uint64_t subFieldOnDiskSize = 0;
   std::uint64_t subFieldInMemorySize = 0;

   for (const auto &subField : inspector->GetDescriptor().GetFieldIterable(topFieldInfo.GetDescriptor().GetId())) {
      auto subFieldInfo = inspector->GetFieldTreeInspector(subField.GetId());
      subFieldOnDiskSize += subFieldInfo.GetCompressedSize();
      subFieldInMemorySize += subFieldInfo.GetUncompressedSize();
   }

   EXPECT_EQ(topFieldInfo.GetCompressedSize(), subFieldOnDiskSize);
   EXPECT_EQ(topFieldInfo.GetUncompressedSize(), subFieldInMemorySize);
}

TEST(RNTupleInspector, FieldTypeCount)
{
   FileRaii fileGuard("test_ntuple_inspector_field_type_count.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldObject = model->MakeField<ComplexStructUtil>("object");
      auto nFldInt1 = model->MakeField<std::int32_t>("int1");
      auto nFldInt2 = model->MakeField<std::int32_t>("int2");
      auto nFldInt3 = model->MakeField<std::int32_t>("int3");
      auto nFldString1 = model->MakeField<std::string>("string1");
      auto nFldString2 = model->MakeField<std::string>("string2");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   EXPECT_EQ(1, inspector->GetFieldCountByType("ComplexStructUtil"));
   EXPECT_EQ(1, inspector->GetFieldCountByType("ComplexStructUtil", false));

   EXPECT_EQ(1, inspector->GetFieldCountByType("std::vector<HitUtil>"));
   EXPECT_EQ(0, inspector->GetFieldCountByType("std::vector<HitUtil>", false));

   EXPECT_EQ(2, inspector->GetFieldCountByType("std::vector<.*>"));
   EXPECT_EQ(0, inspector->GetFieldCountByType("std::vector<.*>", false));

   EXPECT_EQ(3, inspector->GetFieldCountByType("BaseUtil"));
   EXPECT_EQ(0, inspector->GetFieldCountByType("BaseUtil", false));

   EXPECT_EQ(6, inspector->GetFieldCountByType("std::int32_t"));
   EXPECT_EQ(3, inspector->GetFieldCountByType("std::int32_t", false));

   EXPECT_EQ(4, inspector->GetFieldCountByType("float"));
   EXPECT_EQ(0, inspector->GetFieldCountByType("float", false));
}

TEST(RNTupleInspector, FieldsByName)
{
   FileRaii fileGuard("test_ntuple_inspector_fields_by_name.root");
   {
      auto model = RNTupleModel::Create();
      auto nFldInt1 = model->MakeField<std::int32_t>("int1");
      auto nFldInt2 = model->MakeField<std::int32_t>("int2");
      auto nFldInt3 = model->MakeField<std::int32_t>("int3");
      auto nFldFloat1 = model->MakeField<float>("float1");
      auto nFldFloat2 = model->MakeField<float>("float2");

      auto writeOptions = RNTupleWriteOptions();
      writeOptions.SetCompression(505);
      auto ntuple = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath(), writeOptions);
   }

   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());

   auto intFieldIds = inspector->GetFieldsByName("int.");

   EXPECT_EQ(3, intFieldIds.size());
   for (const auto fieldId : intFieldIds) {
      EXPECT_EQ("std::int32_t", inspector->GetFieldTreeInspector(fieldId).GetDescriptor().GetTypeName());
   }
}

TEST(RNTupleInspector, MultiColumnRepresentations)
{
   FileRaii fileGuard("test_ntuple_inspector_multi_column_representations.root");
   {
      auto model = RNTupleModel::Create();
      auto fldPx = RFieldBase::Create("px", "float").Unwrap();
      fldPx->SetColumnRepresentatives({{ENTupleColumnType::kReal32}, {ENTupleColumnType::kReal16}});
      model->AddField(std::move(fldPx));
      auto writer = RNTupleWriter::Recreate(std::move(model), "ntpl", fileGuard.GetPath());
      writer->Fill();
      writer->CommitCluster();
      ROOT::Internal::RFieldRepresentationModifier::SetPrimaryColumnRepresentation(
         const_cast<RFieldBase &>(writer->GetModel().GetConstField("px")), 1);
      writer->Fill();
   }

   auto inspector = RNTupleInspector::Create("ntpl", fileGuard.GetPath());
   auto px0Inspector = inspector->GetColumnInspector(0);
   auto px1Inspector = inspector->GetColumnInspector(1);
   EXPECT_EQ(ENTupleColumnType::kReal32, px0Inspector.GetType());
   EXPECT_EQ(1u, px0Inspector.GetNElements());
   EXPECT_EQ(ENTupleColumnType::kReal16, px1Inspector.GetType());
   EXPECT_EQ(1u, px1Inspector.GetNElements());
}

TEST(RNTupleInspector, FieldTreeAsDot)
{
   FileRaii fileGuard("test_ntuple_inspector_fields_tree_as_dot.root");
   {
      auto model = RNTupleModel::Create();
      auto fldFloat1 = model->MakeField<float>("float1");
      auto fldInt = model->MakeField<std::int32_t>("int");
      auto writer = RNTupleWriter::Recreate(std::move(model), "ntuple", fileGuard.GetPath());
   }
   auto inspector = RNTupleInspector::Create("ntuple", fileGuard.GetPath());
   std::ostringstream dotStream;
   inspector->PrintFieldTreeAsDot(dotStream);
   const std::string dot = dotStream.str();
   const std::string &expected =
      "digraph D {\nnode[shape=box]\n0[label=<<b>RFieldZero</b>>]\n0->1\n1[label=<<b>Name: "
      "</b>float1<br></br><b>Type: </b>float<br></br><b>ID: </b>0<br></br>>]\n0->2\n2[label=<<b>Name: "
      "</b>int<br></br><b>Type: </b>std::int32_t<br></br><b>ID: </b>1<br></br>>]\n}";
   EXPECT_EQ(dot, expected);
}
