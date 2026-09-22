# KritvaOS Community Security Policy

**Project:** KritvaOS Community  
**Repository:** `kritvaos-community`  
**Status:** Community policy v0.1

## 1. Scope

This policy describes how to report and handle security vulnerabilities affecting KritvaOS Community.

KritvaOS is a robotics computing platform. Security issues may affect software, interfaces, build systems, deployment mechanisms, or integrations.

Security reports should focus on reproducible vulnerabilities in KritvaOS Community or code maintained by the project.

## 2. Do Not Publicly Disclose Unfixed Vulnerabilities

Please do not open a public GitHub issue, pull request, discussion, or other public post for an unpatched security vulnerability.

Public disclosure before a fix is available can increase risk to users and deployed robotic systems.

## 3. Reporting a Vulnerability

Until a dedicated private security reporting mechanism is established, security researchers should contact the project maintainers privately using the security contact published in the repository or the project's GitHub Security Advisory mechanism, if enabled.

**Security contact:**  
`<SECURITY_EMAIL_TO_BE_DEFINED>`

When reporting a vulnerability, please provide:

- affected repository and component;
- affected version, commit, or release;
- vulnerability description;
- steps to reproduce;
- proof of concept, where appropriate;
- expected and observed behavior;
- potential security impact;
- any known mitigations or workarounds.

Please do not include unnecessary personal, customer, credential, or production data.

## 4. What to Expect

The maintainers will make reasonable efforts to:

1. acknowledge receipt of a valid report;
2. reproduce and assess the issue;
3. determine affected versions and components;
4. coordinate a fix or mitigation;
5. prepare an appropriate security advisory when disclosure is warranted; and
6. coordinate public disclosure after a fix or mitigation is available.

Response and remediation times may vary depending on severity, complexity, affected components, and availability of maintainers.

## 5. Coordinated Disclosure

KritvaOS Community intends to use coordinated disclosure for vulnerabilities that could materially affect users.

The maintainers may coordinate disclosure timing with the reporter and affected downstream projects or vendors where appropriate.

A security advisory may include:

- affected versions;
- fixed versions;
- severity or impact description;
- mitigation guidance;
- acknowledgements, where the reporter agrees.

## 6. Security-Sensitive Robotics Issues

Because KritvaOS may interact with physical robotic systems, reports involving safety-relevant behavior, unauthorized actuator control, privilege escalation, remote code execution, authentication bypass, insecure communications, or compromise of robot control paths should be treated as potentially high-impact.

Reporters should clearly identify such implications when known.

Security impact should not be assumed solely from the component name; the maintainers will assess the actual affected architecture and deployment context.

## 7. Out-of-Scope Reports

The following generally do not constitute security vulnerabilities in the project by themselves:

- feature requests;
- ordinary bugs without a security impact;
- unsupported configurations;
- vulnerabilities entirely contained in third-party software that are not caused or introduced by KritvaOS;
- publicly documented issues that have already been addressed;
- requests for credentials or access to private project infrastructure.

Third-party vulnerabilities may still be useful to report when they materially affect KritvaOS deployments or when KritvaOS configuration introduces additional exposure.

## 8. Security Updates

Security fixes may be released through normal KritvaOS Community release mechanisms, security advisories, or both.

Users should keep KritvaOS components and relevant dependencies updated and review security advisories applicable to their deployment.

## 9. Enterprise and Downstream Products

KritvaOS Community may be used as a component of KritvaOS Enterprise and other commercial or customer-specific systems.

A vulnerability in Community code may therefore affect downstream products. Maintainers may coordinate with downstream maintainers or vendors when appropriate.

The Apache-2.0 license of KritvaOS Community is unchanged by such downstream use.

## 10. Responsible Research

Security research that follows applicable law and avoids unnecessary disruption, privacy violations, data destruction, or unauthorized access is encouraged.

Researchers should stop testing when sufficient evidence has been obtained to demonstrate the issue and should not access or retain data that is not necessary to demonstrate the vulnerability.

## 11. Policy Changes

This policy may evolve as the KritvaOS Community and security response process mature.

The repository should be updated when a dedicated security contact, private reporting workflow, security advisory process, or response SLA is established.
