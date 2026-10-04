#!/usr/bin/env python3
"""Chapter 49: writes this chapter's INVENTED claims as real-format X12 837P files (data/claim_A.837 ... claim_F.837). Everything about the people, the practice and the charges is made up; the NPI 1234567893 is the check-digit-valid
example NPI used in public CMS documentation. build_837() is also used by native/gen_claims.py to make random claims for the differential test."""
import os
def build_837(claims, ctl=101, date="20240115", npi="1234567893", sub=("EXAMPLE", "PAT", "XYZ123456789"), payer=("ACME HEALTH PLAN", "99999"), practice="RIVERSIDE FAMILY MEDICINE", isa_test="T", extra_per_segment=True):
    """claims: list of dict(pcn, dx=[...], lines=[(code, charge_cents_str, units, dos CCYYMMDD)]). Returns the interchange text."""
    S = []
    S.append(f"BHT*0019*00*CLAIM{ctl}*{date}*0900*CH"); S.append(f"NM1*41*2*{practice}*****46*RIVERSIDEFM"); S.append("PER*IC*BILLING OFFICE*TE*5555550100"); S.append(f"NM1*40*2*{payer[0]}*****46*ACMEHEALTH")
    S.append("HL*1**20*1"); S.append(f"NM1*85*2*{practice}*****XX*{npi}"); S.append("N3*100 MAIN STREET"); S.append("N4*SPRINGFIELD*IL*62701"); S.append("REF*EI*123456789")
    S.append("HL*2*1*22*0"); S.append("SBR*P*18*GROUP123******CI"); S.append(f"NM1*IL*1*{sub[0]}*{sub[1]}****MI*{sub[2]}"); S.append("N3*22 ELM STREET"); S.append("N4*SPRINGFIELD*IL*62701"); S.append("DMG*D8*19800101*F")
    S.append(f"NM1*PR*2*{payer[0]}*****PI*{payer[1]}")
    for c in claims:
        total = sum(int(ch) for _, ch, _, _ in c["lines"])
        S.append(f"CLM*{c['pcn']}*{total // 100}{'' if total % 100 == 0 else '.%02d' % (total % 100)}***11:B:1*Y*A*Y*Y")
        S.append("HI*" + "*".join(("ABK:" if i == 0 else "ABF:") + d for i, d in enumerate(c["dx"])))
        for k, (code, ch, units, dos) in enumerate(c["lines"], 1):
            chs = f"{int(ch) // 100}{'' if int(ch) % 100 == 0 else '.%02d' % (int(ch) % 100)}"
            S.append(f"LX*{k}"); S.append(f"SV1*HC:{code}*{chs}*UN*{units}***1"); S.append(f"DTP*472*D8*{dos}")
    n = len(S) + 2   # ST and SE included
    c9 = "%09d" % ctl; s1 = "RIVERSIDEFM".ljust(15); s2 = "ACMEHEALTH".ljust(15)
    out = [f"ISA*00*          *00*          *ZZ*{s1}*ZZ*{s2}*{date[2:]}*0900*^*00501*{c9}*0*{isa_test}*:", f"GS*HC*RIVERSIDEFM*ACMEHEALTH*{date}*0900*{ctl}*X*005010X222A1", "ST*837*0001*005010X222A1"] + S + [f"SE*{n}*0001", f"GE*1*{ctl}", f"IEA*1*{c9}"]
    return "~\n".join(out) + "~\n"
SCENARIO = [
 ("A", 101, "20240115", [dict(pcn="PCN-A001", dx=["J069"], lines=[("99213", "15000", 1, "20240115"), ("85025", "4000", 1, "20240115"), ("36415", "1200", 1, "20240115")])]),
 ("B", 102, "20240220", [dict(pcn="PCN-B001", dx=["I10", "E119"], lines=[("99214", "21000", 1, "20240220"), ("93000", "6000", 1, "20240220")])]),
 ("C", 103, "20240303", [dict(pcn="PCN-A001", dx=["J069"], lines=[("99213", "15000", 1, "20240115"), ("85025", "4000", 1, "20240115"), ("36415", "1200", 1, "20240115")])]),
 ("D", 104, "20240410", [dict(pcn="PCN-D001", dx=["Z1211"], lines=[("45378", "150000", 1, "20240410"), ("A9999", "3500", 1, "20240410")])]),
 ("E", 105, "20240602", [dict(pcn="PCN-E001", dx=["M1711"], lines=[("27447", "4200000", 1, "20240602")])]),
 ("F", 106, "20240709", [dict(pcn="PCN-F001", dx=["J069"], lines=[("99213", "15000", 1, "20240709")])]),
]
if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__)); os.makedirs(os.path.join(here, "data"), exist_ok=True)
    for name, ctl, date, claims in SCENARIO:
        open(os.path.join(here, "data", f"claim_{name}.837"), "w").write(build_837(claims, ctl, date)); print("wrote claim_" + name + ".837")
