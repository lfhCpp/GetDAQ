#include "root5_tree_fixture.h"

#include "GDataFrame.h"
#include <TBranch.h>
#include <TFile.h>
#include <TTree.h>

#include <stdexcept>

void CreateRoot5NamespaceTreeFixture(const std::string &path, bool invalidChannel)
{
    TFile file(path.c_str(), "RECREATE");
    if (file.IsZombie()) throw std::runtime_error("cannot create ROOT5 namespace fixture");
    TTree tree("tree", "lowercase ROOT5-era object names");
    get::GDataFrame frame;
    TBranch *branch = tree.Branch("GDataFrame", &frame);
    if (!branch) throw std::runtime_error("cannot create lowercase GDataFrame branch");
    if (std::string(branch->GetClassName()) != "get::GDataFrame")
        throw std::runtime_error("fixture dictionary did not preserve the ROOT5-era lowercase class name");
    frame.fHeader.fEventIdx = 29;
    frame.fHeader.fEventTime = 987654321;
    frame.fHeader.fCoboIdx = 3;
    frame.fHeader.fAsadIdx = 2;
    frame.AddSample(invalidChannel ? 4 : 0, 1, 5, 321);
    frame.AddSample(3, 67, 511, 4095);
    if (tree.Fill() < 0) throw std::runtime_error("cannot fill ROOT5 namespace fixture");
    file.cd();
    if (tree.Write() <= 0) throw std::runtime_error("cannot write ROOT5 namespace fixture");
    file.Close();
}
