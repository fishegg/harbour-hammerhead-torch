# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.27
# 

Name:       harbour-hammerhead-torch

# >> macros
# << macros

%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}
Summary:    Torch application for the LG Nexus 5 ("Hammerhead") port of SailfishOS, Jolla and more.
Version:    0.12
Release:    1
Group:      Qt/Qt
License:    GPLv3+
URL:        https://samhobbs.co.uk
Source0:    %{name}-%{version}.tar.bz2
Source100:  harbour-hammerhead-torch.yaml
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   nemo-qml-plugin-policy-qt5
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(gstreamer-1.0) >= 1.4.5
BuildRequires:  nemo-qml-plugin-policy-qt5
BuildRequires:  desktop-file-utils

%description
Hammerhead Torch is a simple torch app originally written for the LG Nexus 5 ("Hammerhead") port of SailfishOS, now supports Jolla and other phones with a simple LED control file.


%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qtc_qmake5 

%qtc_make %{?_smp_mflags}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake5_install

# >> install post
# << install post

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
# >> files
# << files
