/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.tools.lint.checks;

import static com.android.tools.lint.detector.api.LintConstants.ANDROID_MANIFEST_XML;
import static com.android.tools.lint.detector.api.LintConstants.ANDROID_URI;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_EXPORTED;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_PATH;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_PATH_PATTERN;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_PATH_PREFIX;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_PERMISSION;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_READ_PERMISSION;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_WRITE_PERMISSION;
import static com.android.tools.lint.detector.api.LintConstants.TAG_APPLICATION;
import static com.android.tools.lint.detector.api.LintConstants.TAG_GRANT_PERMISSION;
import static com.android.tools.lint.detector.api.LintConstants.TAG_INTENT_FILTER;
import static com.android.tools.lint.detector.api.LintConstants.TAG_PATH_PERMISSION;
import static com.android.tools.lint.detector.api.LintConstants.TAG_PROVIDER;
import static com.android.tools.lint.detector.api.LintConstants.TAG_SERVICE;

import com.android.tools.lint.detector.api.Category;
import com.android.tools.lint.detector.api.Context;
import com.android.tools.lint.detector.api.Detector;
import com.android.tools.lint.detector.api.Issue;
import com.android.tools.lint.detector.api.JavaContext;
import com.android.tools.lint.detector.api.LintUtils;
import com.android.tools.lint.detector.api.Location;
import com.android.tools.lint.detector.api.Scope;
import com.android.tools.lint.detector.api.Severity;
import com.android.tools.lint.detector.api.Speed;
import com.android.tools.lint.detector.api.XmlContext;

import org.w3c.dom.Attr;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.EnumSet;
import java.util.Iterator;
import java.util.List;

import lombok.ast.AstVisitor;
import lombok.ast.Expression;
import lombok.ast.ForwardingAstVisitor;
import lombok.ast.Identifier;
import lombok.ast.MethodInvocation;
import lombok.ast.StrictListAccessor;

/**
 * Checks that exported services request a permission.
 */
public class SecurityDetector extends Detector implements Detector.XmlScanner,
        Detector.JavaScanner {

    /** Exported services */
    public static final Issue EXPORTED_SERVICE = Issue.create(
            "ExportedService", //$NON-NLS-1$
            "Checks for exported services that do not require permissions",
            "Exported services (services which either set exported=true or contain " +
            "an intent-filter and do not specify exported=false) should define a " +
            "permission that an entity must have in order to launch the service " +
            "or bind to it. Without this, any application can use this service.",
            Category.SECURITY,
            5,
            Severity.WARNING,
            SecurityDetector.class,
            EnumSet.of(Scope.MANIFEST));

    /** Exported content providers */
    public static final Issue EXPORTED_PROVIDER = Issue.create(
            "ExportedContentProvider", //$NON-NLS-1$
            "Checks for exported content providers that do not require permissions",
            "Content providers are exported by default and any application on the " +
            "system can potentially use them to read and write data. If the content" +
            "provider provides access to sensitive data, it should be protected by " +
            "specifying export=false in the manifest or by protecting it with a " +
            "permission that can be granted to other applications.",
            Category.SECURITY,
            5,
            Severity.WARNING,
            SecurityDetector.class,
            EnumSet.of(Scope.MANIFEST));

    /** Content provides which grant all URIs access */
    public static final Issue OPEN_PROVIDER = Issue.create(
            "GrantAllUris", //$NON-NLS-1$
            "Checks for <grant-uri-permission> elements where everything is shared",
            "The <grant-uri-permission> element allows specific paths to be shared. " +
            "This detector checks for a path URL of just '/' (everything), which is " +
            "probably not what you want; you should limit access to a subset.",
            Category.SECURITY,
            7,
            Severity.WARNING,
            SecurityDetector.class,
            EnumSet.of(Scope.MANIFEST));

    /** Using the world-writable flag */
    public static final Issue WORLD_WRITEABLE = Issue.create(
            "WorldWriteableFiles", //$NON-NLS-1$
            "Checks for openFileOutput() and getSharedPreferences() calls passing " +
            "MODE_WORLD_WRITEABLE",
            "There are cases where it is appropriate for an application to write " +
            "world writeable files, but these should be reviewed carefully to " +
            "ensure that they contain no private data, and that if the file is " +
            "modified by a malicious application it does not trick or compromise " +
            "your application.",
            Category.SECURITY,
            4,
            Severity.WARNING,
            SecurityDetector.class,
            Scope.JAVA_FILE_SCOPE);


    /** Using the world-readable flag */
    public static final Issue WORLD_READABLE = Issue.create(
            "WorldReadableFiles", //$NON-NLS-1$
            "Checks for openFileOutput() and getSharedPreferences() calls passing " +
            "MODE_WORLD_READABLE",
            "There are cases where it is appropriate for an application to write " +
            "world readable files, but these should be reviewed carefully to " +
            "ensure that they contain no private data that is leaked to other " +
            "applications.",
            Category.SECURITY,
            4,
            Severity.WARNING,
            SecurityDetector.class,
            Scope.JAVA_FILE_SCOPE);

    /** Constructs a new {@link SecurityDetector} check */
    public SecurityDetector() {
    }

    @Override
    public Speed getSpeed() {
        return Speed.FAST;
    }
    @Override
    public boolean appliesTo(Context context, File file) {
        return file.getName().equals(ANDROID_MANIFEST_XML);
    }

    // ---- Implements Detector.XmlScanner ----

    @Override
    public Collection<String> getApplicableElements() {
        return Arrays.asList(
            TAG_SERVICE,
            TAG_GRANT_PERMISSION,
            TAG_PROVIDER
        );
    }

    @Override
    public void visitElement(XmlContext context, Element element) {
        String tag = element.getTagName();
        if (tag.equals(TAG_SERVICE)) {
            checkService(context, element);
        } else if (tag.equals(TAG_GRANT_PERMISSION)) {
            checkGrantPermission(context, element);
        } else if (tag.equals(TAG_PROVIDER)) {
            checkProvider(context, element);
        }
    }

    private void checkService(XmlContext context, Element element) {
        String exportValue = element.getAttributeNS(ANDROID_URI, ATTR_EXPORTED);
        boolean exported;
        if (exportValue != null && exportValue.length() > 0) {
            exported = Boolean.valueOf(exportValue);
        } else {
            boolean haveIntentFilters = false;
            for (Element child : LintUtils.getChildren(element)) {
                if (child.getTagName().equals(TAG_INTENT_FILTER)) {
                    haveIntentFilters = true;
                    break;
                }
            }
            exported = haveIntentFilters;
        }

        if (exported) {
            // Make sure this service has a permission
            String permission = element.getAttributeNS(ANDROID_URI, ATTR_PERMISSION);
            if (permission == null || permission.length() == 0) {
                Node parent = element.getParentNode();
                if (parent.getNodeType() == Node.ELEMENT_NODE
                        && parent.getNodeName().equals(TAG_APPLICATION)) {
                    Element application = (Element) parent;
                    permission = application.getAttributeNS(ANDROID_URI, ATTR_PERMISSION);
                    if (permission == null || permission.length() == 0) {
                        // No declared permission for this exported service: complain
                        context.report(EXPORTED_SERVICE, element,
                            context.getLocation(element),
                            "Exported service does not require permission", null);
                    }
                }
            }
        }
    }

    private void checkGrantPermission(XmlContext context, Element element) {
        Attr path = element.getAttributeNodeNS(ANDROID_URI, ATTR_PATH);
        Attr prefix = element.getAttributeNodeNS(ANDROID_URI, ATTR_PATH_PREFIX);
        Attr pattern = element.getAttributeNodeNS(ANDROID_URI, ATTR_PATH_PATTERN);

        String msg = "Content provider shares everything; this is potentially dangerous.";
        if (path != null && path.getValue().equals("/")) { //$NON-NLS-1$
            context.report(OPEN_PROVIDER, path, context.getLocation(path), msg, null);
        }
        if (prefix != null && prefix.getValue().equals("/")) { //$NON-NLS-1$
            context.report(OPEN_PROVIDER, prefix, context.getLocation(prefix), msg, null);
        }
        if (pattern != null && (pattern.getValue().equals("/")  //$NON-NLS-1$
               /* || pattern.getValue().equals(".*")*/)) {
            context.report(OPEN_PROVIDER, pattern, context.getLocation(pattern), msg, null);
        }
    }

    private void checkProvider(XmlContext context, Element element) {
        String exportValue = element.getAttributeNS(ANDROID_URI, ATTR_EXPORTED);
        // Content providers are exported by default
        boolean exported = true;
        if (exportValue != null && exportValue.length() > 0) {
            exported = Boolean.valueOf(exportValue);
        }

        if (exported) {
            // Just check for some use of permissions. Other Lint checks can check the saneness
            // of the permissions. We'll accept the permission, readPermission, or writePermission
            // attributes on the provider element, or a path-permission element.
            String permission = element.getAttributeNS(ANDROID_URI, ATTR_READ_PERMISSION);
            if (permission == null || permission.length() == 0) {
                permission = element.getAttributeNS(ANDROID_URI, ATTR_WRITE_PERMISSION);
                if (permission == null || permission.length() == 0) {
                    permission = element.getAttributeNS(ANDROID_URI, ATTR_PERMISSION);
                    if (permission == null || permission.length() == 0) {
                        // No permission attributes? Check for path-permission.

                        // TODO: Add a Lint check to ensure the path-permission is good, similar to
                        // the grant-uri-permission check.
                        boolean hasPermission = false;
                        for (Element child : LintUtils.getChildren(element)) {
                            String tag = child.getTagName();
                            if (tag.equals(TAG_PATH_PERMISSION)) {
                                hasPermission = true;
                                break;
                            }
                        }

                        if (!hasPermission) {
                            context.report(EXPORTED_PROVIDER, element, context.getLocation(element),
                                    "Exported content providers can provide access to " +
                                            "potentially sensitive data",
                                    null);
                        }
                    }
                }
            }
        }
    }

    // ---- Implements Detector.JavaScanner ----

    @Override
    public List<String> getApplicableMethodNames() {
        // These are the API calls that can accept a MODE_WORLD_READABLE/MODE_WORLD_WRITABLE
        // argument.
        List<String> values = new ArrayList<String>(2);
        values.add("openFileOutput"); //$NON-NLS-1$
        values.add("getSharedPreferences"); //$NON-NLS-1$
        return values;
    }

    @Override
    public void visitMethod(JavaContext context, AstVisitor visitor, MethodInvocation node) {
        StrictListAccessor<Expression,MethodInvocation> args = node.astArguments();
        Iterator<Expression> iterator = args.iterator();
        while (iterator.hasNext()) {
            iterator.next().accept(visitor);
        }
    }

    @Override
    public AstVisitor createJavaVisitor(JavaContext context) {
        return new IdentifierVisitor(context);
    }

    private static class IdentifierVisitor extends ForwardingAstVisitor {
        private final JavaContext mContext;

        public IdentifierVisitor(JavaContext context) {
            super();
            this.mContext = context;
        }

        @Override
        public boolean visitIdentifier(Identifier node) {
            if ("MODE_WORLD_WRITEABLE".equals(node.getDescription())) { //$NON-NLS-1$
                Location location = mContext.getLocation(node);
                mContext.report(WORLD_WRITEABLE, node, location,
                        "Using MODE_WORLD_WRITEABLE when creating files can be " +
                                "risky, review carefully",
                        null);
            } else if ("MODE_WORLD_READABLE".equals(node.getDescription())) { //$NON-NLS-1$
                Location location = mContext.getLocation(node);
                mContext.report(WORLD_READABLE, node, location,
                        "Using MODE_WORLD_READABLE when creating files can be " +
                                "risky, review carefully",
                        null);
            }

            return false;
        }
    }
}
