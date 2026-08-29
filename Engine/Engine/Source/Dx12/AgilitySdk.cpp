#ifdef _WIN32

#ifndef GSE_D3D12_AGILITY_VERSION
#define GSE_D3D12_AGILITY_VERSION 616
#endif

#if GSE_D3D12_AGILITY_VERSION
extern "C" {
	__declspec(dllexport) extern const unsigned int D3D12SDKVersion = GSE_D3D12_AGILITY_VERSION;
	__declspec(dllexport) const char* D3D12SDKPath = ".\\D3D12\\";
}
#endif

#endif
